/**
 * @file src/starbeam/protocol.h
 * @brief Starbeam protocol message definitions.
 */
#pragma once

#include <string>
#include <map>
#include <optional>
#include <vector>
#include <cstdint>

#include <boost/property_tree/ptree.hpp>

namespace starbeam {
namespace protocol {

  // Message types
  enum class MessageType {
    Register,
    RegisterAck,
    RegisterError,
    HttpRequest,
    HttpResponse,
    RtspRequest,
    RtspResponse,
    UdpChannelSetup,
    UdpChannelAck,
    UdpChannelClose,
    SessionStart,
    SessionEnd,
    Ping,
    Pong,
    Error,
    Unknown
  };

  // Port assignment structure
  struct PortAssignment {
    uint16_t http;
    uint16_t https;
    uint16_t rtsp;
    uint16_t video;
    uint16_t audio;
    uint16_t control;
  };

  // Host capabilities
  struct HostCapabilities {
    std::optional<uint32_t> max_width;
    std::optional<uint32_t> max_height;
    std::optional<uint32_t> max_fps;
    std::vector<std::string> video_codecs;
    std::vector<std::string> audio_codecs;
  };

  // Registration message
  struct RegisterMessage {
    std::string hostname;
    std::string unique_id;
    std::optional<std::string> host_id;
    std::string auth_key;
    HostCapabilities capabilities;

    std::string to_json() const;
  };

  // Registration acknowledgment
  struct RegisterAckMessage {
    std::string host_id;
    PortAssignment ports;
    std::optional<std::string> external_address;

    static RegisterAckMessage from_json(const std::string &json);
  };

  // HTTP request (from Starbeam to Sunlight)
  struct HttpRequestMessage {
    uint64_t id;
    std::string method;
    std::string path;
    std::optional<std::string> query;
    std::map<std::string, std::string> headers;
    std::optional<std::string> body;
    bool is_https;
    std::string client_addr;

    static HttpRequestMessage from_json(const std::string &json);
  };

  // HTTP response (from Sunlight to Starbeam)
  struct HttpResponseMessage {
    uint64_t id;
    uint16_t status;
    std::map<std::string, std::string> headers;
    std::optional<std::string> body;

    std::string to_json() const;
  };

  // RTSP request (from Starbeam to Sunlight)
  struct RtspRequestMessage {
    uint64_t id;
    std::string method;
    std::string uri;
    std::map<std::string, std::string> headers;
    std::optional<std::string> body;
    std::string client_addr;

    static RtspRequestMessage from_json(const std::string &json);
  };

  // RTSP response (from Sunlight to Starbeam)
  struct RtspResponseMessage {
    uint64_t id;
    uint16_t status;
    std::string reason;
    std::map<std::string, std::string> headers;
    std::optional<std::string> body;

    std::string to_json() const;
  };

  // UDP channel types
  enum class UdpChannelType {
    Video,
    Audio,
    Control
  };

  // UDP channel setup (from Starbeam to Sunlight)
  struct UdpChannelSetupMessage {
    uint64_t session_id;
    UdpChannelType channel;
    std::string client_addr;

    static UdpChannelSetupMessage from_json(const std::string &json);
  };

  // UDP channel acknowledgment (from Sunlight to Starbeam)
  struct UdpChannelAckMessage {
    uint64_t session_id;
    UdpChannelType channel;
    uint16_t relay_port;
    uint16_t local_port;

    std::string to_json() const;
  };

  // Session start notification
  struct SessionStartMessage {
    uint64_t session_id;
    std::string client_id;
    std::string client_addr;

    static SessionStartMessage from_json(const std::string &json);
  };

  // Session end notification
  struct SessionEndMessage {
    uint64_t session_id;
    std::optional<std::string> reason;

    std::string to_json() const;
  };

  // Ping message
  struct PingMessage {
    uint64_t ts;

    static PingMessage from_json(const std::string &json);
  };

  // Pong message
  struct PongMessage {
    uint64_t ts;

    std::string to_json() const;
  };

  // Error message
  struct ErrorMessage {
    std::string code;
    std::string message;
    std::optional<uint64_t> request_id;

    static ErrorMessage from_json(const std::string &json);
    std::string to_json() const;
  };

  // Parse message type from JSON
  MessageType parse_message_type(const std::string &json);

  // Helper to get message type string
  std::string message_type_string(MessageType type);

  // Helper to convert UdpChannelType to/from string
  std::string channel_type_string(UdpChannelType type);
  UdpChannelType channel_type_from_string(const std::string &str);

}  // namespace protocol
}  // namespace starbeam
