/**
 * @file src/starbeam/protocol.cpp
 * @brief Starbeam protocol message implementation.
 */
#include "protocol.h"

#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace pt = boost::property_tree;

namespace starbeam {
namespace protocol {

  // Helper to escape JSON strings
  std::string escape_json(const std::string &s) {
    std::ostringstream o;
    for (auto c : s) {
      switch (c) {
        case '"': o << "\\\""; break;
        case '\\': o << "\\\\"; break;
        case '\b': o << "\\b"; break;
        case '\f': o << "\\f"; break;
        case '\n': o << "\\n"; break;
        case '\r': o << "\\r"; break;
        case '\t': o << "\\t"; break;
        default:
          if ('\x00' <= c && c <= '\x1f') {
            o << "\\u"
              << std::hex << std::setw(4) << std::setfill('0') << (int) c;
          } else {
            o << c;
          }
      }
    }
    return o.str();
  }

  std::string RegisterMessage::to_json() const {
    std::ostringstream ss;
    ss << "{\"type\":\"register\""
       << ",\"hostname\":\"" << escape_json(hostname) << "\""
       << ",\"unique_id\":\"" << escape_json(unique_id) << "\""
       << ",\"auth_key\":\"" << escape_json(auth_key) << "\"";

    if (host_id) {
      ss << ",\"host_id\":\"" << escape_json(*host_id) << "\"";
    }

    ss << ",\"capabilities\":{";
    if (capabilities.max_width) {
      ss << "\"max_width\":" << *capabilities.max_width << ",";
    }
    if (capabilities.max_height) {
      ss << "\"max_height\":" << *capabilities.max_height << ",";
    }
    if (capabilities.max_fps) {
      ss << "\"max_fps\":" << *capabilities.max_fps << ",";
    }

    ss << "\"video_codecs\":[";
    for (size_t i = 0; i < capabilities.video_codecs.size(); ++i) {
      if (i > 0) ss << ",";
      ss << "\"" << escape_json(capabilities.video_codecs[i]) << "\"";
    }
    ss << "],\"audio_codecs\":[";
    for (size_t i = 0; i < capabilities.audio_codecs.size(); ++i) {
      if (i > 0) ss << ",";
      ss << "\"" << escape_json(capabilities.audio_codecs[i]) << "\"";
    }
    ss << "]}}";

    return ss.str();
  }

  RegisterAckMessage RegisterAckMessage::from_json(const std::string &json) {
    std::istringstream ss(json);
    pt::ptree tree;
    pt::read_json(ss, tree);

    RegisterAckMessage msg;
    msg.host_id = tree.get<std::string>("host_id");

    auto ports_tree = tree.get_child("ports");
    msg.ports.http = ports_tree.get<uint16_t>("http");
    msg.ports.https = ports_tree.get<uint16_t>("https");
    msg.ports.rtsp = ports_tree.get<uint16_t>("rtsp");
    msg.ports.video = ports_tree.get<uint16_t>("video");
    msg.ports.audio = ports_tree.get<uint16_t>("audio");
    msg.ports.control = ports_tree.get<uint16_t>("control");

    if (auto ext = tree.get_optional<std::string>("external_address")) {
      msg.external_address = *ext;
    }

    return msg;
  }

  HttpRequestMessage HttpRequestMessage::from_json(const std::string &json) {
    std::istringstream ss(json);
    pt::ptree tree;
    pt::read_json(ss, tree);

    HttpRequestMessage msg;
    msg.id = tree.get<uint64_t>("id");
    msg.method = tree.get<std::string>("method");
    msg.path = tree.get<std::string>("path");

    if (auto q = tree.get_optional<std::string>("query")) {
      msg.query = *q;
    }

    if (auto headers = tree.get_child_optional("headers")) {
      for (auto &[key, value] : *headers) {
        msg.headers[key] = value.get_value<std::string>();
      }
    }

    if (auto body = tree.get_optional<std::string>("body")) {
      msg.body = *body;
    }

    msg.is_https = tree.get<bool>("is_https", false);
    msg.client_addr = tree.get<std::string>("client_addr");

    return msg;
  }

  std::string HttpResponseMessage::to_json() const {
    std::ostringstream ss;
    ss << "{\"type\":\"http_response\""
       << ",\"id\":" << id
       << ",\"status\":" << status
       << ",\"headers\":{";

    bool first = true;
    for (auto &[key, value] : headers) {
      if (!first) ss << ",";
      ss << "\"" << escape_json(key) << "\":\"" << escape_json(value) << "\"";
      first = false;
    }
    ss << "}";

    if (body) {
      ss << ",\"body\":\"" << escape_json(*body) << "\"";
    }

    ss << "}";
    return ss.str();
  }

  RtspRequestMessage RtspRequestMessage::from_json(const std::string &json) {
    std::istringstream ss(json);
    pt::ptree tree;
    pt::read_json(ss, tree);

    RtspRequestMessage msg;
    msg.id = tree.get<uint64_t>("id");
    msg.method = tree.get<std::string>("method");
    msg.uri = tree.get<std::string>("uri");

    if (auto headers = tree.get_child_optional("headers")) {
      for (auto &[key, value] : *headers) {
        msg.headers[key] = value.get_value<std::string>();
      }
    }

    if (auto body = tree.get_optional<std::string>("body")) {
      msg.body = *body;
    }

    msg.client_addr = tree.get<std::string>("client_addr");

    return msg;
  }

  std::string RtspResponseMessage::to_json() const {
    std::ostringstream ss;
    ss << "{\"type\":\"rtsp_response\""
       << ",\"id\":" << id
       << ",\"status\":" << status
       << ",\"reason\":\"" << escape_json(reason) << "\""
       << ",\"headers\":{";

    bool first = true;
    for (auto &[key, value] : headers) {
      if (!first) ss << ",";
      ss << "\"" << escape_json(key) << "\":\"" << escape_json(value) << "\"";
      first = false;
    }
    ss << "}";

    if (body) {
      ss << ",\"body\":\"" << escape_json(*body) << "\"";
    }

    ss << "}";
    return ss.str();
  }

  UdpChannelSetupMessage UdpChannelSetupMessage::from_json(const std::string &json) {
    std::istringstream ss(json);
    pt::ptree tree;
    pt::read_json(ss, tree);

    UdpChannelSetupMessage msg;
    msg.session_id = tree.get<uint64_t>("session_id");
    msg.channel = channel_type_from_string(tree.get<std::string>("channel"));
    msg.client_addr = tree.get<std::string>("client_addr");

    return msg;
  }

  std::string UdpChannelAckMessage::to_json() const {
    std::ostringstream ss;
    ss << "{\"type\":\"udp_channel_ack\""
       << ",\"session_id\":" << session_id
       << ",\"channel\":\"" << channel_type_string(channel) << "\""
       << ",\"relay_port\":" << relay_port
       << ",\"local_port\":" << local_port
       << "}";
    return ss.str();
  }

  SessionStartMessage SessionStartMessage::from_json(const std::string &json) {
    std::istringstream ss(json);
    pt::ptree tree;
    pt::read_json(ss, tree);

    SessionStartMessage msg;
    msg.session_id = tree.get<uint64_t>("session_id");
    msg.client_id = tree.get<std::string>("client_id");
    msg.client_addr = tree.get<std::string>("client_addr");

    return msg;
  }

  std::string SessionEndMessage::to_json() const {
    std::ostringstream ss;
    ss << "{\"type\":\"session_end\""
       << ",\"session_id\":" << session_id;
    if (reason) {
      ss << ",\"reason\":\"" << escape_json(*reason) << "\"";
    }
    ss << "}";
    return ss.str();
  }

  PingMessage PingMessage::from_json(const std::string &json) {
    std::istringstream ss(json);
    pt::ptree tree;
    pt::read_json(ss, tree);

    PingMessage msg;
    msg.ts = tree.get<uint64_t>("ts");
    return msg;
  }

  std::string PongMessage::to_json() const {
    std::ostringstream ss;
    ss << "{\"type\":\"pong\",\"ts\":" << ts << "}";
    return ss.str();
  }

  ErrorMessage ErrorMessage::from_json(const std::string &json) {
    std::istringstream ss(json);
    pt::ptree tree;
    pt::read_json(ss, tree);

    ErrorMessage msg;
    msg.code = tree.get<std::string>("code");
    msg.message = tree.get<std::string>("message");

    if (auto req_id = tree.get_optional<uint64_t>("request_id")) {
      msg.request_id = *req_id;
    }

    return msg;
  }

  std::string ErrorMessage::to_json() const {
    std::ostringstream ss;
    ss << "{\"type\":\"error\""
       << ",\"code\":\"" << escape_json(code) << "\""
       << ",\"message\":\"" << escape_json(message) << "\"";
    if (request_id) {
      ss << ",\"request_id\":" << *request_id;
    }
    ss << "}";
    return ss.str();
  }

  MessageType parse_message_type(const std::string &json) {
    try {
      std::istringstream ss(json);
      pt::ptree tree;
      pt::read_json(ss, tree);

      auto type_str = tree.get<std::string>("type");

      if (type_str == "register") return MessageType::Register;
      if (type_str == "register_ack") return MessageType::RegisterAck;
      if (type_str == "register_error") return MessageType::RegisterError;
      if (type_str == "http_request") return MessageType::HttpRequest;
      if (type_str == "http_response") return MessageType::HttpResponse;
      if (type_str == "rtsp_request") return MessageType::RtspRequest;
      if (type_str == "rtsp_response") return MessageType::RtspResponse;
      if (type_str == "udp_channel_setup") return MessageType::UdpChannelSetup;
      if (type_str == "udp_channel_ack") return MessageType::UdpChannelAck;
      if (type_str == "udp_channel_close") return MessageType::UdpChannelClose;
      if (type_str == "session_start") return MessageType::SessionStart;
      if (type_str == "session_end") return MessageType::SessionEnd;
      if (type_str == "ping") return MessageType::Ping;
      if (type_str == "pong") return MessageType::Pong;
      if (type_str == "error") return MessageType::Error;

      return MessageType::Unknown;
    } catch (...) {
      return MessageType::Unknown;
    }
  }

  std::string message_type_string(MessageType type) {
    switch (type) {
      case MessageType::Register: return "register";
      case MessageType::RegisterAck: return "register_ack";
      case MessageType::RegisterError: return "register_error";
      case MessageType::HttpRequest: return "http_request";
      case MessageType::HttpResponse: return "http_response";
      case MessageType::RtspRequest: return "rtsp_request";
      case MessageType::RtspResponse: return "rtsp_response";
      case MessageType::UdpChannelSetup: return "udp_channel_setup";
      case MessageType::UdpChannelAck: return "udp_channel_ack";
      case MessageType::UdpChannelClose: return "udp_channel_close";
      case MessageType::SessionStart: return "session_start";
      case MessageType::SessionEnd: return "session_end";
      case MessageType::Ping: return "ping";
      case MessageType::Pong: return "pong";
      case MessageType::Error: return "error";
      default: return "unknown";
    }
  }

  std::string channel_type_string(UdpChannelType type) {
    switch (type) {
      case UdpChannelType::Video: return "video";
      case UdpChannelType::Audio: return "audio";
      case UdpChannelType::Control: return "control";
      default: return "unknown";
    }
  }

  UdpChannelType channel_type_from_string(const std::string &str) {
    if (str == "video") return UdpChannelType::Video;
    if (str == "audio") return UdpChannelType::Audio;
    if (str == "control") return UdpChannelType::Control;
    return UdpChannelType::Video;  // Default
  }

}  // namespace protocol
}  // namespace starbeam
