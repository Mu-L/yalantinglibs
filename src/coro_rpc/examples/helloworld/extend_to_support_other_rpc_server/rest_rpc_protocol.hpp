/*
 * Copyright (c) 2023, Alibaba Group Holding Limited;
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <variant>

#include "coro_rpc/coro_rpc/default_config/coro_rpc_config.hpp"
#include "coro_rpc/coro_rpc/router.hpp"
#include "msgpack_protocol.hpp"

namespace coro_rpc {
namespace protocol {
struct rest_rpc_protocol {
  enum class request_type : uint8_t { req_res, sub_pub };
  struct req_header {
    uint8_t magic;
    request_type req_type;
    uint32_t body_len;
    uint64_t req_id;
    uint32_t func_id;
  };
  using resp_header = req_header;
  using supported_serialize_protocols = std::variant<msgpack_protocol>;
  using router = coro_rpc::protocol::router<rest_rpc_protocol>;
  using route_key_t = uint32_t;

  static route_key_t get_route_key(req_header& req_header) {
    return req_header.func_id;
  };

  static std::optional<supported_serialize_protocols> get_serialize_protocol(
      req_header& req_header) {
    return msgpack_protocol{};
  }

  template <typename Socket>
  static async_simple::coro::Lazy<std::error_code> read_head(
      Socket& socket, req_header& req_head) {
    auto [ec, _] = co_await asio_util::async_read(
        socket, asio::buffer((char*)&req_head, sizeof(req_header)));
    if (ec) [[unlikely]] {
      co_return std::move(ec);
    }
    else if (req_head.magic != magic_number) [[unlikely]] {
      co_return std::make_error_code(std::errc::protocol_error);
    }
    co_return std::error_code{};
  }

  template <typename Socket, typename Buffer>
  static async_simple::coro::Lazy<std::error_code> read_payload(
      Socket& socket, req_header& req_head, Buffer& buffer) {
    buffer.resize(req_head.body_len);
    auto [ec, _] = co_await asio_util::async_read(socket, asio::buffer(buffer));
    co_return ec;
  }

  static std::string prepare_response(std::string& rpc_result,
                                      const req_header& req_header,
                                      std::errc rpc_err_code = {},
                                      std::string_view err_msg = {}) {
    std::string header_buf;
    header_buf.resize(RESP_HEAD_LEN);
    auto& resp_head = *(resp_header*)header_buf.data();
    resp_head = req_header;
    resp_head.body_len = rpc_result.size();
    return header_buf;
  }

  constexpr static inline int8_t magic_number = 39;
  static constexpr auto REQ_HEAD_LEN = sizeof(req_header{});
  static constexpr auto RESP_HEAD_LEN = sizeof(resp_header{});
};
}  // namespace protocol
}  // namespace coro_rpc

namespace coro_rpc::config {
struct rest_rpc_config : public coro_rpc_config_base {
  using rpc_protocol = coro_rpc::protocol::rest_rpc_protocol;
};
}  // namespace coro_rpc::config