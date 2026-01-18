/**
 * @file src/starbeam/udp.h
 * @brief Starbeam UDP channel relay for video/audio/control streams.
 */
#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <boost/asio.hpp>

#include "protocol.h"

namespace starbeam {
namespace udp {

  /**
   * @brief Manages UDP channels for relaying video/audio/control streams.
   *
   * When streaming through Starbeam, the relay allocates UDP ports that clients
   * connect to. This class creates local UDP sockets that forward data between
   * Sunshine's local streaming and the Starbeam relay server.
   */
  class ChannelManager {
  public:
    ChannelManager();
    ~ChannelManager();

    /**
     * @brief Initialize the channel manager.
     * @param relay_host Starbeam relay server hostname
     * @param relay_video_port Relay's video port
     * @param relay_audio_port Relay's audio port
     * @param relay_control_port Relay's control port
     * @return true if initialization succeeded
     */
    bool initialize(
      const std::string &relay_host,
      uint16_t relay_video_port,
      uint16_t relay_audio_port,
      uint16_t relay_control_port
    );

    /**
     * @brief Shutdown all UDP channels.
     */
    void shutdown();

    /**
     * @brief Handle a UDP channel setup request from Starbeam.
     * @param setup The channel setup message
     * @return Channel acknowledgment with local port info
     */
    protocol::UdpChannelAckMessage handle_channel_setup(
      const protocol::UdpChannelSetupMessage &setup
    );

    /**
     * @brief Get local port for a channel type.
     * @param channel Channel type
     * @return Local port or 0 if not set up
     */
    uint16_t get_local_port(protocol::UdpChannelType channel) const;

    /**
     * @brief Check if channel manager is running.
     * @return true if running
     */
    bool is_running() const;

  private:
    struct Channel {
      std::unique_ptr<boost::asio::ip::udp::socket> socket;
      boost::asio::ip::udp::endpoint relay_endpoint;
      boost::asio::ip::udp::endpoint local_endpoint;
      uint16_t local_port;
      std::thread relay_thread;
      std::atomic<bool> running{false};
    };

    void run_relay(protocol::UdpChannelType type);
    uint16_t get_sunshine_port(protocol::UdpChannelType type);

    std::unique_ptr<boost::asio::io_context> io_context_;
    std::map<protocol::UdpChannelType, std::unique_ptr<Channel>> channels_;
    mutable std::mutex mutex_;
    std::atomic<bool> running_{false};

    std::string relay_host_;
    uint16_t relay_video_port_ = 0;
    uint16_t relay_audio_port_ = 0;
    uint16_t relay_control_port_ = 0;
  };

  /**
   * @brief Get the global channel manager instance.
   * @return Reference to the channel manager
   */
  ChannelManager &get_channel_manager();

  /**
   * @brief Initialize UDP channel support.
   * @return true if successful
   */
  bool initialize();

  /**
   * @brief Shutdown UDP channel support.
   */
  void shutdown();

  /**
   * @brief Handle UDP channel setup callback for starbeam client.
   * @param setup Channel setup message
   * @return Channel acknowledgment message
   */
  protocol::UdpChannelAckMessage handle_channel_setup(
    const protocol::UdpChannelSetupMessage &setup
  );

}  // namespace udp
}  // namespace starbeam
