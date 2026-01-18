/**
 * @file src/starbeam/tunnel.cpp
 * @brief Starbeam tunnel implementation.
 */
#include "tunnel.h"
#include "client.h"
#include "udp.h"

#include "../logging.h"

namespace starbeam {
namespace tunnel {

  static NvhttpHandler g_nvhttp_handler;
  static RtspHandler g_rtsp_handler;
  static std::mutex g_handler_mutex;

  // Internal HTTP request handler that forwards to nvhttp
  static protocol::HttpResponseMessage handle_http_request(const protocol::HttpRequestMessage &req) {
    protocol::HttpResponseMessage resp;
    resp.id = req.id;

    std::lock_guard<std::mutex> lock(g_handler_mutex);
    if (!g_nvhttp_handler) {
      BOOST_LOG(error) << "starbeam::tunnel: No HTTP handler registered";
      resp.status = 500;
      resp.body = "Internal Server Error: No handler";
      return resp;
    }

    try {
      auto [status, content_type, body] = g_nvhttp_handler(
        req.method,
        req.path,
        req.query.value_or(""),
        req.headers,
        req.body.value_or(""),
        req.client_addr,
        req.is_https
      );

      resp.status = status;
      if (!content_type.empty()) {
        resp.headers["Content-Type"] = content_type;
      }
      if (!body.empty()) {
        resp.body = body;
      }
    } catch (const std::exception &e) {
      BOOST_LOG(error) << "starbeam::tunnel: HTTP handler error: " << e.what();
      resp.status = 500;
      resp.body = "Internal Server Error";
    }

    return resp;
  }

  // Internal RTSP request handler that forwards to rtsp module
  static protocol::RtspResponseMessage handle_rtsp_request(const protocol::RtspRequestMessage &req) {
    protocol::RtspResponseMessage resp;
    resp.id = req.id;

    std::lock_guard<std::mutex> lock(g_handler_mutex);
    if (!g_rtsp_handler) {
      BOOST_LOG(error) << "starbeam::tunnel: No RTSP handler registered";
      resp.status = 500;
      resp.reason = "Internal Server Error";
      return resp;
    }

    try {
      auto [status, reason, headers, body] = g_rtsp_handler(
        req.method,
        req.uri,
        req.headers,
        req.body.value_or(""),
        req.client_addr
      );

      resp.status = status;
      resp.reason = reason;
      resp.headers = headers;
      if (!body.empty()) {
        resp.body = body;
      }
    } catch (const std::exception &e) {
      BOOST_LOG(error) << "starbeam::tunnel: RTSP handler error: " << e.what();
      resp.status = 500;
      resp.reason = "Internal Server Error";
    }

    return resp;
  }

  bool initialize() {
    auto client = get_client();
    if (!client) {
      BOOST_LOG(warning) << "starbeam::tunnel: No client available";
      return false;
    }

    // Register our handlers with the client
    client->set_http_handler(handle_http_request);
    client->set_rtsp_handler(handle_rtsp_request);
    client->set_udp_channel_handler(udp::handle_channel_setup);

    BOOST_LOG(info) << "starbeam::tunnel: Initialized";
    return true;
  }

  void shutdown() {
    // Clear handlers
    std::lock_guard<std::mutex> lock(g_handler_mutex);
    g_nvhttp_handler = nullptr;
    g_rtsp_handler = nullptr;
    BOOST_LOG(info) << "starbeam::tunnel: Shutdown";
  }

  void set_nvhttp_handler(NvhttpHandler handler) {
    std::lock_guard<std::mutex> lock(g_handler_mutex);
    g_nvhttp_handler = std::move(handler);
  }

  void set_rtsp_handler(RtspHandler handler) {
    std::lock_guard<std::mutex> lock(g_handler_mutex);
    g_rtsp_handler = std::move(handler);
  }

  bool is_active() {
    auto client = get_client();
    return client && client->is_ready();
  }

}  // namespace tunnel
}  // namespace starbeam
