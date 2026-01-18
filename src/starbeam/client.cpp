/**
 * @file src/starbeam/client.cpp
 * @brief Starbeam WebSocket client implementation.
 */
#include "client.h"
#include "udp.h"

#include <chrono>
#include <regex>

#include "../config.h"
#include "../logging.h"
#include "../platform/common.h"

namespace starbeam {

  using namespace std::chrono_literals;

  // Global client instance
  static std::shared_ptr<Client> g_client;
  static std::mutex g_client_mutex;

  Client::Client(const std::string &server_url, const std::string &auth_key, const std::string &host_id)
      : server_url_(server_url), auth_key_(auth_key), host_id_(host_id) {
    hostname_ = platf::get_host_name();
    // Generate a unique ID based on hostname and time
    unique_id_ = hostname_ + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
  }

  Client::~Client() {
    stop();
  }

  bool Client::start() {
    if (running_) {
      return true;
    }

    running_ = true;

    // Start IO thread
    io_thread_ = std::make_unique<std::thread>([this]() {
      run_io_context();
    });

    return true;
  }

  void Client::stop() {
    running_ = false;

    if (io_context_) {
      io_context_->stop();
    }

    if (io_thread_ && io_thread_->joinable()) {
      io_thread_->join();
    }

    io_thread_.reset();
    set_state(ConnectionState::Disconnected);
  }

  bool Client::is_ready() const {
    return state_ == ConnectionState::Registered;
  }

  ConnectionState Client::get_state() const {
    return state_;
  }

  std::string Client::get_host_id() const {
    return assigned_host_id_;
  }

  protocol::PortAssignment Client::get_ports() const {
    return assigned_ports_;
  }

  void Client::set_hostname(const std::string &hostname) {
    hostname_ = hostname;
  }

  void Client::set_unique_id(const std::string &unique_id) {
    unique_id_ = unique_id;
  }

  void Client::set_http_handler(HttpRequestHandler handler) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    http_handler_ = std::move(handler);
  }

  void Client::set_rtsp_handler(RtspRequestHandler handler) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    rtsp_handler_ = std::move(handler);
  }

  void Client::set_session_start_handler(SessionStartHandler handler) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    session_start_handler_ = std::move(handler);
  }

  void Client::set_session_end_handler(SessionEndHandler handler) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    session_end_handler_ = std::move(handler);
  }

  void Client::set_udp_channel_handler(UdpChannelSetupHandler handler) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    udp_channel_handler_ = std::move(handler);
  }

  void Client::set_state_handler(StateChangeHandler handler) {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    state_handler_ = std::move(handler);
  }

  void Client::send_session_end(uint64_t session_id, const std::string &reason) {
    protocol::SessionEndMessage msg;
    msg.session_id = session_id;
    if (!reason.empty()) {
      msg.reason = reason;
    }
    send_message(msg.to_json());
  }

  void Client::set_reconnect_interval(int seconds) {
    reconnect_interval_seconds_ = seconds;
  }

  bool Client::parse_url(const std::string &url, std::string &host, std::string &port, std::string &path, bool &use_ssl) {
    // Parse URL like wss://example.com:8443/path or ws://example.com:8080
    std::regex url_regex(R"((wss?):\/\/([^:/]+)(?::(\d+))?(\/.*)?)", std::regex::icase);
    std::smatch match;

    if (!std::regex_match(url, match, url_regex)) {
      BOOST_LOG(error) << "starbeam: Invalid URL format: " << url;
      return false;
    }

    std::string scheme = match[1].str();
    use_ssl = (scheme == "wss" || scheme == "WSS");

    host = match[2].str();
    port = match[3].matched ? match[3].str() : (use_ssl ? "443" : "80");
    path = match[4].matched ? match[4].str() : "/";

    return true;
  }

  void Client::run_io_context() {
    while (running_) {
      try {
        connect();

        if (state_ == ConnectionState::Connected || state_ == ConnectionState::Registered) {
          // Run the IO context
          io_context_->run();
        }
      } catch (const std::exception &e) {
        BOOST_LOG(error) << "starbeam: Connection error: " << e.what();
      }

      // Cleanup
      disconnect();

      if (running_) {
        // Wait before reconnecting
        BOOST_LOG(info) << "starbeam: Reconnecting in " << reconnect_interval_seconds_ << " seconds...";
        for (int i = 0; i < reconnect_interval_seconds_ && running_; ++i) {
          std::this_thread::sleep_for(1s);
        }
      }
    }
  }

  void Client::connect() {
    set_state(ConnectionState::Connecting);

    std::string host, port, path;
    if (!parse_url(server_url_, host, port, path, use_ssl_)) {
      set_state(ConnectionState::Error);
      return;
    }

    BOOST_LOG(info) << "starbeam: Connecting to " << server_url_;

    // Create new IO context
    io_context_ = std::make_unique<net::io_context>();

    // Resolve the host
    tcp::resolver resolver(*io_context_);
    auto results = resolver.resolve(host, port);

    if (use_ssl_) {
      // SSL connection
      ssl_context_ = std::make_unique<ssl::context>(ssl::context::tlsv12_client);
      ssl_context_->set_default_verify_paths();
      ssl_context_->set_verify_mode(ssl::verify_none);  // TODO: Enable proper verification

      wss_ = std::make_unique<websocket::stream<beast::ssl_stream<tcp::socket>>>(*io_context_, *ssl_context_);

      // Connect to the server
      auto &socket = beast::get_lowest_layer(*wss_);
      socket.connect(*results.begin());

      // Set SNI hostname
      if (!SSL_set_tlsext_host_name(wss_->next_layer().native_handle(), host.c_str())) {
        throw beast::system_error(
          beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()),
          "Failed to set SNI hostname"
        );
      }

      // SSL handshake
      wss_->next_layer().handshake(ssl::stream_base::client);

      // WebSocket handshake
      wss_->handshake(host, path);

      BOOST_LOG(info) << "starbeam: Connected (WSS)";
    } else {
      // Plain WebSocket connection
      ws_ = std::make_unique<websocket::stream<tcp::socket>>(*io_context_);

      // Connect to the server
      auto &socket = beast::get_lowest_layer(*ws_);
      socket.connect(*results.begin());

      // WebSocket handshake
      ws_->handshake(host, path);

      BOOST_LOG(info) << "starbeam: Connected (WS)";
    }

    set_state(ConnectionState::Connected);

    // Send registration
    send_registration();

    // Start reading
    do_read();
  }

  void Client::disconnect() {
    try {
      if (use_ssl_ && wss_) {
        wss_->close(websocket::close_code::normal);
        wss_.reset();
      } else if (ws_) {
        ws_->close(websocket::close_code::normal);
        ws_.reset();
      }
    } catch (...) {
      // Ignore errors during close
    }

    wss_.reset();
    ws_.reset();
    ssl_context_.reset();
    io_context_.reset();

    assigned_host_id_.clear();
    assigned_ports_ = {};

    set_state(ConnectionState::Disconnected);
  }

  void Client::send_registration() {
    protocol::RegisterMessage reg;
    reg.hostname = hostname_;
    reg.unique_id = unique_id_;
    reg.auth_key = auth_key_;

    if (!host_id_.empty()) {
      reg.host_id = host_id_;
    }

    // Add capabilities
    reg.capabilities.video_codecs = {"H264", "HEVC", "AV1"};
    reg.capabilities.audio_codecs = {"opus"};

    send_message(reg.to_json());
    BOOST_LOG(info) << "starbeam: Sent registration as '" << hostname_ << "'";
  }

  void Client::do_read() {
    auto self = this;

    auto read_handler = [self](beast::error_code ec, std::size_t bytes_transferred) {
      if (ec) {
        if (ec != websocket::error::closed) {
          BOOST_LOG(error) << "starbeam: Read error: " << ec.message();
        }
        self->io_context_->stop();
        return;
      }

      std::string message = beast::buffers_to_string(self->read_buffer_.data());
      self->read_buffer_.consume(bytes_transferred);

      self->handle_message(message);

      // Continue reading
      if (self->running_) {
        self->do_read();
      }
    };

    if (use_ssl_ && wss_) {
      wss_->async_read(read_buffer_, read_handler);
    } else if (ws_) {
      ws_->async_read(read_buffer_, read_handler);
    }
  }

  void Client::handle_message(const std::string &message) {
    auto msg_type = protocol::parse_message_type(message);

    switch (msg_type) {
      case protocol::MessageType::RegisterAck: {
        auto ack = protocol::RegisterAckMessage::from_json(message);
        assigned_host_id_ = ack.host_id;
        assigned_ports_ = ack.ports;
        set_state(ConnectionState::Registered);
        BOOST_LOG(info) << "starbeam: Registered as '" << assigned_host_id_
                        << "' with HTTP port " << assigned_ports_.http;

        // Initialize UDP channel manager with relay port info
        // Extract host from server URL for UDP connections
        std::string relay_host, port, path;
        bool use_ssl;
        if (parse_url(server_url_, relay_host, port, path, use_ssl)) {
          udp::get_channel_manager().initialize(
            relay_host,
            assigned_ports_.video,
            assigned_ports_.audio,
            assigned_ports_.control
          );
        }
        break;
      }

      case protocol::MessageType::RegisterError: {
        auto err = protocol::ErrorMessage::from_json(message);
        BOOST_LOG(error) << "starbeam: Registration failed: " << err.message;
        set_state(ConnectionState::Error);
        io_context_->stop();
        break;
      }

      case protocol::MessageType::HttpRequest: {
        std::lock_guard<std::mutex> lock(handler_mutex_);
        if (http_handler_) {
          auto req = protocol::HttpRequestMessage::from_json(message);
          auto resp = http_handler_(req);
          send_message(resp.to_json());
        }
        break;
      }

      case protocol::MessageType::RtspRequest: {
        std::lock_guard<std::mutex> lock(handler_mutex_);
        if (rtsp_handler_) {
          auto req = protocol::RtspRequestMessage::from_json(message);
          auto resp = rtsp_handler_(req);
          send_message(resp.to_json());
        }
        break;
      }

      case protocol::MessageType::SessionStart: {
        std::lock_guard<std::mutex> lock(handler_mutex_);
        if (session_start_handler_) {
          auto msg = protocol::SessionStartMessage::from_json(message);
          session_start_handler_(msg);
        }
        break;
      }

      case protocol::MessageType::SessionEnd: {
        std::lock_guard<std::mutex> lock(handler_mutex_);
        if (session_end_handler_) {
          // Parse session_id from message
          std::istringstream ss(message);
          boost::property_tree::ptree tree;
          boost::property_tree::read_json(ss, tree);
          auto session_id = tree.get<uint64_t>("session_id");
          session_end_handler_(session_id);
        }
        break;
      }

      case protocol::MessageType::UdpChannelSetup: {
        std::lock_guard<std::mutex> lock(handler_mutex_);
        if (udp_channel_handler_) {
          auto setup = protocol::UdpChannelSetupMessage::from_json(message);
          auto ack = udp_channel_handler_(setup);
          send_message(ack.to_json());
        }
        break;
      }

      case protocol::MessageType::Ping: {
        auto ping = protocol::PingMessage::from_json(message);
        protocol::PongMessage pong;
        pong.ts = ping.ts;
        send_message(pong.to_json());
        break;
      }

      case protocol::MessageType::Error: {
        auto err = protocol::ErrorMessage::from_json(message);
        BOOST_LOG(error) << "starbeam: Error from server: " << err.code << " - " << err.message;
        break;
      }

      default:
        BOOST_LOG(warning) << "starbeam: Unknown message type";
        break;
    }
  }

  void Client::send_message(const std::string &message) {
    try {
      if (use_ssl_ && wss_) {
        wss_->write(net::buffer(message));
      } else if (ws_) {
        ws_->write(net::buffer(message));
      }
    } catch (const std::exception &e) {
      BOOST_LOG(error) << "starbeam: Send error: " << e.what();
    }
  }

  void Client::set_state(ConnectionState new_state) {
    auto old_state = state_.exchange(new_state);
    if (old_state != new_state) {
      std::lock_guard<std::mutex> lock(handler_mutex_);
      if (state_handler_) {
        state_handler_(old_state, new_state);
      }
    }
  }

  // Global functions
  std::shared_ptr<Client> get_client() {
    std::lock_guard<std::mutex> lock(g_client_mutex);
    return g_client;
  }

  bool initialize() {
    if (!is_enabled()) {
      BOOST_LOG(info) << "starbeam: Disabled in configuration";
      return false;
    }

    std::lock_guard<std::mutex> lock(g_client_mutex);

    if (g_client) {
      BOOST_LOG(warning) << "starbeam: Already initialized";
      return true;
    }

    auto &cfg = config::starbeam;

    if (cfg.server_url.empty()) {
      BOOST_LOG(error) << "starbeam: Server URL not configured";
      return false;
    }

    if (cfg.auth_key.empty()) {
      BOOST_LOG(error) << "starbeam: Auth key not configured";
      return false;
    }

    g_client = std::make_shared<Client>(cfg.server_url, cfg.auth_key, cfg.host_id);
    g_client->set_reconnect_interval(cfg.reconnect_interval_seconds);

    // Set hostname from Sunshine config
    g_client->set_hostname(config::nvhttp.sunshine_name);

    if (!g_client->start()) {
      BOOST_LOG(error) << "starbeam: Failed to start client";
      g_client.reset();
      return false;
    }

    BOOST_LOG(info) << "starbeam: Initialized and connecting to " << cfg.server_url;
    return true;
  }

  void shutdown() {
    std::lock_guard<std::mutex> lock(g_client_mutex);
    if (g_client) {
      g_client->stop();
      g_client.reset();
    }
    BOOST_LOG(info) << "starbeam: Shutdown complete";
  }

  bool is_enabled() {
    return config::starbeam.enabled;
  }

}  // namespace starbeam
