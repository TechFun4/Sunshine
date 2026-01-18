/**
 * @file src/starbeam/tunnel.h
 * @brief Starbeam tunnel for routing HTTP/RTSP through relay.
 */
#pragma once

#include <functional>
#include <string>
#include <map>

#include "protocol.h"

namespace starbeam {
namespace tunnel {

  /**
   * @brief HTTP handler signature matching nvhttp's request handling.
   *
   * The handler receives HTTP request details and should return a response.
   */
  using NvhttpHandler = std::function<std::tuple<int, std::string, std::string>(
    const std::string &method,
    const std::string &path,
    const std::string &query,
    const std::map<std::string, std::string> &headers,
    const std::string &body,
    const std::string &client_addr,
    bool is_https
  )>;

  /**
   * @brief RTSP handler signature.
   */
  using RtspHandler = std::function<std::tuple<int, std::string, std::map<std::string, std::string>, std::string>(
    const std::string &method,
    const std::string &uri,
    const std::map<std::string, std::string> &headers,
    const std::string &body,
    const std::string &client_addr
  )>;

  /**
   * @brief Initialize the tunnel and connect handlers to Starbeam client.
   * @return true if successful
   */
  bool initialize();

  /**
   * @brief Shutdown the tunnel.
   */
  void shutdown();

  /**
   * @brief Set the HTTP handler for forwarded requests.
   * @param handler Handler function
   */
  void set_nvhttp_handler(NvhttpHandler handler);

  /**
   * @brief Set the RTSP handler for forwarded requests.
   * @param handler Handler function
   */
  void set_rtsp_handler(RtspHandler handler);

  /**
   * @brief Check if tunnel is active and ready.
   * @return true if ready
   */
  bool is_active();

}  // namespace tunnel
}  // namespace starbeam
