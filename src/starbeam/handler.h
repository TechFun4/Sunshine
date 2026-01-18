/**
 * @file src/starbeam/handler.h
 * @brief Starbeam HTTP/RTSP request handlers.
 */
#pragma once

#include <string>
#include <map>
#include <tuple>

namespace starbeam {
namespace handler {

  /**
   * @brief Handle an HTTP request from the Starbeam relay.
   *
   * This function processes HTTP requests forwarded from Moonlight clients
   * via the Starbeam relay and returns the appropriate response.
   *
   * @param method HTTP method (GET, POST, etc.)
   * @param path Request path (e.g., "/serverinfo")
   * @param query Query string (without leading ?)
   * @param headers Request headers
   * @param body Request body
   * @param client_addr Client's address (from Starbeam)
   * @param is_https Whether this was originally an HTTPS request
   * @return Tuple of (status_code, content_type, body)
   */
  std::tuple<int, std::string, std::string> handle_http_request(
    const std::string &method,
    const std::string &path,
    const std::string &query,
    const std::map<std::string, std::string> &headers,
    const std::string &body,
    const std::string &client_addr,
    bool is_https
  );

  /**
   * @brief Handle an RTSP request from the Starbeam relay.
   *
   * @param method RTSP method (OPTIONS, DESCRIBE, SETUP, etc.)
   * @param uri Request URI
   * @param headers Request headers
   * @param body Request body
   * @param client_addr Client's address
   * @return Tuple of (status_code, reason, headers, body)
   */
  std::tuple<int, std::string, std::map<std::string, std::string>, std::string> handle_rtsp_request(
    const std::string &method,
    const std::string &uri,
    const std::map<std::string, std::string> &headers,
    const std::string &body,
    const std::string &client_addr
  );

  /**
   * @brief Initialize handlers and register with tunnel.
   * @return true if successful
   */
  bool initialize();

  /**
   * @brief Shutdown handlers.
   */
  void shutdown();

}  // namespace handler
}  // namespace starbeam
