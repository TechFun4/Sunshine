/**
 * @file src/starbeam/udp.cpp
 * @brief Starbeam UDP channel relay implementation.
 */
#include "udp.h"

#include "../config.h"
#include "../logging.h"

namespace starbeam {
namespace udp {

  // Global channel manager
  static std::unique_ptr<ChannelManager> g_channel_manager;
  static std::mutex g_manager_mutex;

  ChannelManager::ChannelManager() {}

  ChannelManager::~ChannelManager() {
    shutdown();
  }

  bool ChannelManager::initialize(
    const std::string &relay_host,
    uint16_t relay_video_port,
    uint16_t relay_audio_port,
    uint16_t relay_control_port
  ) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (running_) {
      return true;
    }

    relay_host_ = relay_host;
    relay_video_port_ = relay_video_port;
    relay_audio_port_ = relay_audio_port;
    relay_control_port_ = relay_control_port;

    io_context_ = std::make_unique<boost::asio::io_context>();
    running_ = true;

    BOOST_LOG(info) << "starbeam::udp: Initialized with relay " << relay_host
                    << " (video:" << relay_video_port
                    << " audio:" << relay_audio_port
                    << " control:" << relay_control_port << ")";

    return true;
  }

  void ChannelManager::shutdown() {
    running_ = false;

    std::lock_guard<std::mutex> lock(mutex_);

    for (auto &[type, channel] : channels_) {
      if (channel) {
        channel->running = false;
        if (channel->socket) {
          boost::system::error_code ec;
          channel->socket->close(ec);
        }
        if (channel->relay_thread.joinable()) {
          channel->relay_thread.join();
        }
      }
    }

    channels_.clear();
    io_context_.reset();

    BOOST_LOG(info) << "starbeam::udp: Shutdown complete";
  }

  protocol::UdpChannelAckMessage ChannelManager::handle_channel_setup(
    const protocol::UdpChannelSetupMessage &setup
  ) {
    protocol::UdpChannelAckMessage ack;
    ack.session_id = setup.session_id;
    ack.channel = setup.channel;
    ack.relay_port = 0;
    ack.local_port = 0;

    if (!running_) {
      BOOST_LOG(error) << "starbeam::udp: Channel manager not running";
      return ack;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Get the Sunshine port for this channel type
    uint16_t sunshine_port = get_sunshine_port(setup.channel);
    if (sunshine_port == 0) {
      BOOST_LOG(error) << "starbeam::udp: Unknown channel type";
      return ack;
    }

    // Determine relay port based on channel type
    uint16_t relay_port = 0;
    switch (setup.channel) {
      case protocol::UdpChannelType::Video:
        relay_port = relay_video_port_;
        break;
      case protocol::UdpChannelType::Audio:
        relay_port = relay_audio_port_;
        break;
      case protocol::UdpChannelType::Control:
        relay_port = relay_control_port_;
        break;
    }

    // Check if channel already exists
    auto it = channels_.find(setup.channel);
    if (it != channels_.end() && it->second) {
      // Return existing channel info
      ack.relay_port = relay_port;
      ack.local_port = it->second->local_port;
      return ack;
    }

    // Create new channel
    auto channel = std::make_unique<Channel>();

    try {
      // Create UDP socket bound to any available port
      channel->socket = std::make_unique<boost::asio::ip::udp::socket>(
        *io_context_,
        boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)
      );

      // Get the local port we bound to
      channel->local_port = channel->socket->local_endpoint().port();

      // Set up relay endpoint (starbeam relay server)
      boost::asio::ip::udp::resolver resolver(*io_context_);
      auto endpoints = resolver.resolve(relay_host_, std::to_string(relay_port));
      channel->relay_endpoint = *endpoints.begin();

      // Set up local sunshine endpoint
      channel->local_endpoint = boost::asio::ip::udp::endpoint(
        boost::asio::ip::make_address("127.0.0.1"),
        sunshine_port
      );

      channel->running = true;

      // Start relay thread
      auto channel_type = setup.channel;
      channel->relay_thread = std::thread([this, channel_type]() {
        run_relay(channel_type);
      });

      ack.relay_port = relay_port;
      ack.local_port = channel->local_port;

      BOOST_LOG(info) << "starbeam::udp: Created " << protocol::channel_type_string(setup.channel)
                      << " channel (local:" << channel->local_port
                      << " -> relay:" << relay_port << ")";

      channels_[setup.channel] = std::move(channel);

    } catch (const std::exception &e) {
      BOOST_LOG(error) << "starbeam::udp: Failed to create channel: " << e.what();
    }

    return ack;
  }

  uint16_t ChannelManager::get_local_port(protocol::UdpChannelType channel) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channels_.find(channel);
    if (it != channels_.end() && it->second) {
      return it->second->local_port;
    }
    return 0;
  }

  bool ChannelManager::is_running() const {
    return running_;
  }

  void ChannelManager::run_relay(protocol::UdpChannelType type) {
    Channel *channel = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = channels_.find(type);
      if (it == channels_.end() || !it->second) {
        return;
      }
      channel = it->second.get();
    }

    std::vector<uint8_t> buffer(65535);
    boost::asio::ip::udp::endpoint sender_endpoint;

    while (channel->running && running_) {
      try {
        boost::system::error_code ec;
        size_t len = channel->socket->receive_from(
          boost::asio::buffer(buffer), sender_endpoint, 0, ec);

        if (ec) {
          if (ec != boost::asio::error::operation_aborted) {
            BOOST_LOG(warning) << "starbeam::udp: Receive error: " << ec.message();
          }
          break;
        }

        // Determine where to forward the packet
        boost::asio::ip::udp::endpoint dest;
        if (sender_endpoint.address() == channel->relay_endpoint.address()) {
          // From relay -> forward to local Sunshine
          dest = channel->local_endpoint;
        } else {
          // From local Sunshine -> forward to relay
          dest = channel->relay_endpoint;
        }

        channel->socket->send_to(boost::asio::buffer(buffer.data(), len), dest, 0, ec);
        if (ec) {
          BOOST_LOG(warning) << "starbeam::udp: Send error: " << ec.message();
        }

      } catch (const std::exception &e) {
        BOOST_LOG(error) << "starbeam::udp: Relay error: " << e.what();
        break;
      }
    }
  }

  uint16_t ChannelManager::get_sunshine_port(protocol::UdpChannelType type) {
    // Base port from config
    int base_port = config::sunshine.port;

    switch (type) {
      case protocol::UdpChannelType::Video:
        // Video port is base + 9
        return base_port + 9;
      case protocol::UdpChannelType::Audio:
        // Audio port is base + 10
        return base_port + 10;
      case protocol::UdpChannelType::Control:
        // Control port is base + 8
        return base_port + 8;
      default:
        return 0;
    }
  }

  // Global functions

  ChannelManager &get_channel_manager() {
    std::lock_guard<std::mutex> lock(g_manager_mutex);
    if (!g_channel_manager) {
      g_channel_manager = std::make_unique<ChannelManager>();
    }
    return *g_channel_manager;
  }

  bool initialize() {
    // This will be called when we receive port assignments from starbeam
    // For now, just ensure the manager exists
    get_channel_manager();
    BOOST_LOG(info) << "starbeam::udp: Ready for channel setup";
    return true;
  }

  void shutdown() {
    std::lock_guard<std::mutex> lock(g_manager_mutex);
    if (g_channel_manager) {
      g_channel_manager->shutdown();
      g_channel_manager.reset();
    }
  }

  protocol::UdpChannelAckMessage handle_channel_setup(
    const protocol::UdpChannelSetupMessage &setup
  ) {
    return get_channel_manager().handle_channel_setup(setup);
  }

}  // namespace udp
}  // namespace starbeam
