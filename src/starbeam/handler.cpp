/**
 * @file src/starbeam/handler.cpp
 * @brief Starbeam HTTP/RTSP request handler implementation.
 */
#include "handler.h"
#include "tunnel.h"

#include <sstream>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include "../config.h"
#include "../logging.h"
#include "../network.h"
#include "../nvhttp.h"
#include "../rtsp.h"

namespace starbeam {
namespace handler {

  namespace asio = boost::asio;
  using tcp = asio::ip::tcp;

  // Forward HTTP request to local nvhttp server
  std::tuple<int, std::string, std::string> handle_http_request(
    const std::string &method,
    const std::string &path,
    const std::string &query,
    const std::map<std::string, std::string> &headers,
    const std::string &body,
    const std::string &client_addr,
    bool is_https
  ) {
    try {
      // Connect to local nvhttp server
      asio::io_context io_context;
      tcp::socket socket(io_context);

      // Determine local port using net::map_port
      uint16_t local_port = net::map_port(is_https ? nvhttp::PORT_HTTPS : nvhttp::PORT_HTTP);

      BOOST_LOG(debug) << "starbeam::handler: Connecting to local "
                       << (is_https ? "HTTPS" : "HTTP") << " server at 127.0.0.1:" << local_port;

      tcp::resolver resolver(io_context);
      auto endpoints = resolver.resolve("127.0.0.1", std::to_string(local_port));

      asio::connect(socket, endpoints);

      // Build HTTP request
      std::ostringstream request_stream;
      std::string full_path = path;
      if (!query.empty()) {
        full_path += "?" + query;
      }

      request_stream << method << " " << full_path << " HTTP/1.1\r\n";
      request_stream << "Host: 127.0.0.1:" << local_port << "\r\n";

      // Forward headers (but override some)
      for (const auto &[key, value] : headers) {
        std::string lower_key = key;
        std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);

        // Skip connection-related headers
        if (lower_key == "host" || lower_key == "connection" || lower_key == "transfer-encoding") {
          continue;
        }
        request_stream << key << ": " << value << "\r\n";
      }

      // Add client address as custom header for tracking
      request_stream << "X-Forwarded-For: " << client_addr << "\r\n";
      request_stream << "X-Starbeam-Client: " << client_addr << "\r\n";

      // Add body if present
      if (!body.empty()) {
        request_stream << "Content-Length: " << body.size() << "\r\n";
      }

      request_stream << "Connection: close\r\n";
      request_stream << "\r\n";

      if (!body.empty()) {
        request_stream << body;
      }

      std::string request = request_stream.str();

      // Send request
      asio::write(socket, asio::buffer(request));

      // Read response
      asio::streambuf response_buf;
      boost::system::error_code ec;

      // Read until we get headers
      asio::read_until(socket, response_buf, "\r\n\r\n", ec);

      std::istream response_stream(&response_buf);

      // Parse status line
      std::string http_version;
      int status_code;
      std::string status_message;
      response_stream >> http_version >> status_code;
      std::getline(response_stream, status_message);

      // Parse headers
      std::string content_type;
      size_t content_length = 0;
      std::string header_line;
      while (std::getline(response_stream, header_line) && header_line != "\r") {
        // Remove trailing \r
        if (!header_line.empty() && header_line.back() == '\r') {
          header_line.pop_back();
        }
        if (header_line.empty()) break;

        auto colon_pos = header_line.find(':');
        if (colon_pos != std::string::npos) {
          std::string key = header_line.substr(0, colon_pos);
          std::string value = header_line.substr(colon_pos + 1);
          // Trim whitespace
          while (!value.empty() && value[0] == ' ') value.erase(0, 1);

          std::string lower_key = key;
          std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);

          if (lower_key == "content-type") {
            content_type = value;
          } else if (lower_key == "content-length") {
            content_length = std::stoul(value);
          }
        }
      }

      // Read body
      std::string response_body;
      if (content_length > 0) {
        // Some data might already be in the buffer
        if (response_buf.size() > 0) {
          std::ostringstream body_stream;
          body_stream << &response_buf;
          response_body = body_stream.str();
        }

        // Read remaining body
        while (response_body.size() < content_length) {
          size_t to_read = content_length - response_body.size();
          std::vector<char> buf(to_read);
          size_t n = socket.read_some(asio::buffer(buf), ec);
          if (ec) break;
          response_body.append(buf.data(), n);
        }
      } else {
        // Read until EOF
        std::ostringstream body_stream;
        if (response_buf.size() > 0) {
          body_stream << &response_buf;
        }
        while (asio::read(socket, response_buf, asio::transfer_at_least(1), ec)) {
          body_stream << &response_buf;
        }
        response_body = body_stream.str();
      }

      socket.close();

      BOOST_LOG(debug) << "starbeam::handler: HTTP " << method << " " << path
                       << " -> " << status_code;

      return {status_code, content_type, response_body};

    } catch (const std::exception &e) {
      BOOST_LOG(error) << "starbeam::handler: HTTP request failed: " << e.what();
      return {500, "text/plain", "Internal Server Error"};
    }
  }

  // Forward RTSP request to local RTSP server
  std::tuple<int, std::string, std::map<std::string, std::string>, std::string> handle_rtsp_request(
    const std::string &method,
    const std::string &uri,
    const std::map<std::string, std::string> &headers,
    const std::string &body,
    const std::string &client_addr
  ) {
    try {
      // Connect to local RTSP server
      asio::io_context io_context;
      tcp::socket socket(io_context);

      // RTSP port using net::map_port
      uint16_t rtsp_port = net::map_port(rtsp_stream::RTSP_SETUP_PORT);

      BOOST_LOG(debug) << "starbeam::handler: Connecting to local RTSP server at 127.0.0.1:" << rtsp_port;

      tcp::resolver resolver(io_context);
      auto endpoints = resolver.resolve("127.0.0.1", std::to_string(rtsp_port));

      asio::connect(socket, endpoints);

      // Build RTSP request
      std::ostringstream request_stream;
      request_stream << method << " " << uri << " RTSP/1.0\r\n";

      // Forward headers
      for (const auto &[key, value] : headers) {
        request_stream << key << ": " << value << "\r\n";
      }

      // Add client tracking header
      request_stream << "X-Starbeam-Client: " << client_addr << "\r\n";

      if (!body.empty()) {
        request_stream << "Content-Length: " << body.size() << "\r\n";
      }

      request_stream << "\r\n";

      if (!body.empty()) {
        request_stream << body;
      }

      std::string request = request_stream.str();

      // Send request
      asio::write(socket, asio::buffer(request));

      // Read response
      asio::streambuf response_buf;
      boost::system::error_code ec;

      asio::read_until(socket, response_buf, "\r\n\r\n", ec);

      std::istream response_stream(&response_buf);

      // Parse status line
      std::string rtsp_version;
      int status_code;
      std::string reason;
      response_stream >> rtsp_version >> status_code;
      std::getline(response_stream, reason);
      // Trim leading space and trailing \r
      if (!reason.empty() && reason[0] == ' ') reason.erase(0, 1);
      if (!reason.empty() && reason.back() == '\r') reason.pop_back();

      // Parse headers
      std::map<std::string, std::string> response_headers;
      size_t content_length = 0;
      std::string header_line;
      while (std::getline(response_stream, header_line) && header_line != "\r") {
        if (!header_line.empty() && header_line.back() == '\r') {
          header_line.pop_back();
        }
        if (header_line.empty()) break;

        auto colon_pos = header_line.find(':');
        if (colon_pos != std::string::npos) {
          std::string key = header_line.substr(0, colon_pos);
          std::string value = header_line.substr(colon_pos + 1);
          while (!value.empty() && value[0] == ' ') value.erase(0, 1);

          response_headers[key] = value;

          std::string lower_key = key;
          std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);
          if (lower_key == "content-length") {
            content_length = std::stoul(value);
          }
        }
      }

      // Read body
      std::string response_body;
      if (content_length > 0) {
        if (response_buf.size() > 0) {
          std::ostringstream body_stream;
          body_stream << &response_buf;
          response_body = body_stream.str();
        }
        while (response_body.size() < content_length) {
          size_t to_read = content_length - response_body.size();
          std::vector<char> buf(to_read);
          size_t n = socket.read_some(asio::buffer(buf), ec);
          if (ec) break;
          response_body.append(buf.data(), n);
        }
      }

      socket.close();

      BOOST_LOG(debug) << "starbeam::handler: RTSP " << method << " " << uri
                       << " -> " << status_code;

      return {status_code, reason, response_headers, response_body};

    } catch (const std::exception &e) {
      BOOST_LOG(error) << "starbeam::handler: RTSP request failed: " << e.what();
      return {500, "Internal Server Error", {}, ""};
    }
  }

  bool initialize() {
    // Register handlers with the tunnel
    tunnel::set_nvhttp_handler(handle_http_request);
    tunnel::set_rtsp_handler(handle_rtsp_request);

    BOOST_LOG(info) << "starbeam::handler: Initialized";
    return true;
  }

  void shutdown() {
    // Handlers will be cleared by tunnel::shutdown()
    BOOST_LOG(info) << "starbeam::handler: Shutdown";
  }

}  // namespace handler
}  // namespace starbeam
