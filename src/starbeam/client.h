/**
 * @file src/starbeam/client.h
 * @brief Starbeam WebSocket client for relay connections.
 */
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <queue>
#include <condition_variable>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include "protocol.h"

namespace starbeam {

  namespace beast = boost::beast;
  namespace websocket = beast::websocket;
  namespace net = boost::asio;
  namespace ssl = boost::asio::ssl;
  using tcp = boost::asio::ip::tcp;

  // Connection state
  enum class ConnectionState {
    Disconnected,  ///< Not connected to relay server
    Connecting,    ///< Connection attempt in progress
    Connected,     ///< WebSocket connected, awaiting registration
    Registered,    ///< Successfully registered with relay server
    Error          ///< Connection error occurred
  };

  // Callback types
  using HttpRequestHandler = std::function<protocol::HttpResponseMessage(const protocol::HttpRequestMessage &)>;
  using RtspRequestHandler = std::function<protocol::RtspResponseMessage(const protocol::RtspRequestMessage &)>;
  using SessionStartHandler = std::function<void(const protocol::SessionStartMessage &)>;
  using SessionEndHandler = std::function<void(uint64_t session_id)>;
  using UdpChannelSetupHandler = std::function<protocol::UdpChannelAckMessage(const protocol::UdpChannelSetupMessage &)>;
  using StateChangeHandler = std::function<void(ConnectionState old_state, ConnectionState new_state)>;

  /**
   * @brief Starbeam WebSocket client for connecting to relay server.
   */
  class Client {
  public:
    /**
     * @brief Construct a new Client.
     * @param server_url WebSocket URL (ws:// or wss://)
     * @param auth_key Authentication key
     * @param host_id Optional fixed host ID
     */
    Client(const std::string &server_url, const std::string &auth_key, const std::string &host_id = "");

    /**
     * @brief Destructor - stops the client.
     */
    ~Client();

    /**
     * @brief Start the client (connects in background thread).
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the client and disconnect.
     */
    void stop();

    /**
     * @brief Check if client is connected and registered.
     * @return true if ready to handle requests
     */
    bool is_ready() const;

    /**
     * @brief Get current connection state.
     * @return Connection state
     */
    ConnectionState get_state() const;

    /**
     * @brief Get assigned host ID after registration.
     * @return Host ID or empty string if not registered
     */
    std::string get_host_id() const;

    /**
     * @brief Get assigned ports after registration.
     * @return Port assignment (all zeros if not registered)
     */
    protocol::PortAssignment get_ports() const;

    /**
     * @brief Set hostname for registration.
     * @param hostname Display hostname
     */
    void set_hostname(const std::string &hostname);

    /**
     * @brief Set unique ID for registration.
     * @param unique_id Unique identifier
     */
    void set_unique_id(const std::string &unique_id);

    /**
     * @brief Set HTTP request handler.
     * @param handler Handler function
     */
    void set_http_handler(HttpRequestHandler handler);

    /**
     * @brief Set RTSP request handler.
     * @param handler Handler function
     */
    void set_rtsp_handler(RtspRequestHandler handler);

    /**
     * @brief Set session start handler.
     * @param handler Handler function
     */
    void set_session_start_handler(SessionStartHandler handler);

    /**
     * @brief Set session end handler.
     * @param handler Handler function
     */
    void set_session_end_handler(SessionEndHandler handler);

    /**
     * @brief Set UDP channel setup handler.
     * @param handler Handler function
     */
    void set_udp_channel_handler(UdpChannelSetupHandler handler);

    /**
     * @brief Set state change handler.
     * @param handler Handler function
     */
    void set_state_handler(StateChangeHandler handler);

    /**
     * @brief Send a session end notification.
     * @param session_id Session ID
     * @param reason Optional reason
     */
    void send_session_end(uint64_t session_id, const std::string &reason = "");

    /**
     * @brief Set reconnect interval.
     * @param seconds Interval in seconds
     */
    void set_reconnect_interval(int seconds);

  private:
    // Connection management
    void connect();
    void disconnect();
    void reconnect();
    void run_io_context();

    // Message handling
    void do_read();
    void handle_message(const std::string &message);
    void send_message(const std::string &message);

    // Registration
    void send_registration();

    // State management
    void set_state(ConnectionState new_state);

    // URL parsing
    bool parse_url(const std::string &url, std::string &host, std::string &port, std::string &path, bool &use_ssl);

    // Configuration
    std::string server_url_;
    std::string auth_key_;
    std::string host_id_;
    std::string hostname_;
    std::string unique_id_;
    int reconnect_interval_seconds_ = 5;

    // State
    std::atomic<ConnectionState> state_{ConnectionState::Disconnected};
    std::atomic<bool> running_{false};
    std::string assigned_host_id_;
    protocol::PortAssignment assigned_ports_{};

    // IO
    std::unique_ptr<net::io_context> io_context_;
    std::unique_ptr<ssl::context> ssl_context_;
    std::unique_ptr<websocket::stream<beast::ssl_stream<tcp::socket>>> wss_;
    std::unique_ptr<websocket::stream<tcp::socket>> ws_;
    beast::flat_buffer read_buffer_;
    bool use_ssl_ = false;

    // Threading
    std::unique_ptr<std::thread> io_thread_;
    std::mutex send_mutex_;
    std::queue<std::string> send_queue_;
    std::condition_variable send_cv_;

    // Handlers
    HttpRequestHandler http_handler_;
    RtspRequestHandler rtsp_handler_;
    SessionStartHandler session_start_handler_;
    SessionEndHandler session_end_handler_;
    UdpChannelSetupHandler udp_channel_handler_;
    StateChangeHandler state_handler_;

    mutable std::mutex handler_mutex_;
  };

  /**
   * @brief Get the global Starbeam client instance.
   * @return Shared pointer to client (may be null if not configured)
   */
  std::shared_ptr<Client> get_client();

  /**
   * @brief Initialize the global Starbeam client.
   * @return true if initialized successfully
   */
  bool initialize();

  /**
   * @brief Shutdown the global Starbeam client.
   */
  void shutdown();

  /**
   * @brief Check if Starbeam is enabled in config.
   * @return true if enabled
   */
  bool is_enabled();

}  // namespace starbeam
