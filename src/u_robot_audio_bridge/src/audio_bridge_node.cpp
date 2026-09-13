#include "u_robot_audio_bridge/audio_ipc.hpp"

#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <diagnostic_msgs/msg/diagnostic_status.hpp>
#include <diagnostic_msgs/msg/key_value.hpp>
#include <rcl_interfaces/msg/set_parameters_result.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/empty.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_srvs/srv/set_bool.hpp>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

using namespace std::chrono_literals;

class AudioBridgeNode : public rclcpp::Node {
 public:
  AudioBridgeNode() : Node("audio_bridge") {
    dry_run_ = declare_parameter("dry_run", true);
    output_enabled_ = declare_parameter("output_enabled_on_start", false);
    speaker_id_ = static_cast<int>(declare_parameter("speaker_id", 0));
    volume_ = static_cast<int>(declare_parameter("volume", 80));
    speech_rate_ = declare_parameter("speech_rate", 1.0);
    loop_enabled_ = declare_parameter("loop_enabled", false);
    messages_ = declare_parameter<std::vector<std::string>>("messages", {"大家好，我是宇树机器人。"});
    loop_message_ = declare_parameter("loop_message", messages_.empty() ? "" : messages_.front());
    interval_sec_ = declare_parameter("interval_sec", 10.0);
    repeat_count_ = static_cast<int>(declare_parameter("repeat_count", 0));
    guard_sec_ = declare_parameter("minimum_speech_guard_sec", 4.0);
    max_text_bytes_ = static_cast<int>(declare_parameter("max_text_bytes", 500));
    backend_socket_ = declare_parameter("backend_socket", "/tmp/u_robot_a2_audio.sock");
    reply_socket_ = declare_parameter("reply_socket", "/tmp/u_robot_audio_bridge_reply.sock");
    persistent_config_file_ = declare_parameter("persistent_config_file", "");

    auto status_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    status_pub_ = create_publisher<std_msgs::msg::String>("/audio/status", status_qos);
    diagnostics_pub_ = create_publisher<diagnostic_msgs::msg::DiagnosticArray>("/diagnostics", 10);
    speak_sub_ = create_subscription<std_msgs::msg::String>("/operator/audio/speak", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) { request_speak(msg->data, "manual"); });
    stop_sub_ = create_subscription<std_msgs::msg::Empty>("/operator/audio/stop", 10,
      [this](const std_msgs::msg::Empty::SharedPtr) { stop(); });
    enable_srv_ = create_service<std_srvs::srv::SetBool>("/audio_bridge/enable_output",
      [this](const std_srvs::srv::SetBool::Request::SharedPtr request,
             std_srvs::srv::SetBool::Response::SharedPtr response) {
        output_enabled_ = request->data;
        response->success = true;
        response->message = output_enabled_ ? "audio output enabled" : "audio output disabled";
        if (!output_enabled_) stop(); else send_volume();
        publish_status();
      });
    voice_service_srv_ = create_service<std_srvs::srv::SetBool>("/audio_bridge/enable_voice_service",
      [this](const std_srvs::srv::SetBool::Request::SharedPtr request,
             std_srvs::srv::SetBool::Response::SharedPtr response) {
        u_robot_audio_bridge::AudioCommand command{};
        command.type = u_robot_audio_bridge::AudioCommandType::voice_service_switch;
        command.volume = request->data ? 1U : 0U;
        response->success = send_command(command, true);
        response->message = request->data ?
          "vui_service enable request sent; check backend log/status" :
          "vui_service disable request sent; check backend log/status";
        publish_status();
      });
    parameter_callback_ = add_on_set_parameters_callback(
      std::bind(&AudioBridgeNode::on_parameters, this, std::placeholders::_1));
    open_reply_socket();
    timer_ = create_wall_timer(100ms, std::bind(&AudioBridgeNode::tick, this));
    diagnostics_timer_ = create_wall_timer(2s, std::bind(&AudioBridgeNode::publish_diagnostics, this));
    next_loop_ = now() + rclcpp::Duration::from_seconds(interval_sec_);
    publish_status();
  }

  ~AudioBridgeNode() override {
    if (socket_fd_ >= 0) ::close(socket_fd_);
    ::unlink(reply_socket_.c_str());
  }

 private:
  void open_reply_socket() {
    ::unlink(reply_socket_.c_str());
    socket_fd_ = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (socket_fd_ < 0) throw std::runtime_error("cannot create audio IPC socket");
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (reply_socket_.size() >= sizeof(address.sun_path)) throw std::runtime_error("reply_socket is too long");
    std::memcpy(address.sun_path, reply_socket_.c_str(), reply_socket_.size() + 1);
    if (::bind(socket_fd_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
      throw std::runtime_error("cannot bind audio IPC reply socket");
  }

  rcl_interfaces::msg::SetParametersResult on_parameters(const std::vector<rclcpp::Parameter> & params) {
    rcl_interfaces::msg::SetParametersResult result;
    result.successful = true;
    for (const auto & param : params) {
      const auto & name = param.get_name();
      if (name == "speaker_id" && (param.as_int() < 0 || param.as_int() > 1)) {
        result.successful = false; result.reason = "speaker_id must be 0 (Chinese) or 1 (English)";
      } else if (name == "volume" && (param.as_int() < 0 || param.as_int() > 100)) {
        result.successful = false; result.reason = "volume must be within 0..100";
      } else if (name == "speech_rate" && std::abs(param.as_double() - 1.0) > 0.001) {
        result.successful = false; result.reason = "A2 firmware TTS has no speech-rate API; only 1.0 is supported";
      } else if (name == "interval_sec" && param.as_double() < 0.5) {
        result.successful = false; result.reason = "interval_sec must be at least 0.5";
      } else if (name == "minimum_speech_guard_sec" && param.as_double() < 0.0) {
        result.successful = false; result.reason = "minimum_speech_guard_sec cannot be negative";
      } else if (name == "repeat_count" && param.as_int() < 0) {
        result.successful = false; result.reason = "repeat_count cannot be negative";
      }
    }
    if (!result.successful) return result;
    for (const auto & param : params) {
      const auto & name = param.get_name();
      if (name == "speaker_id") speaker_id_ = static_cast<int>(param.as_int());
      else if (name == "volume") { volume_ = static_cast<int>(param.as_int()); send_volume(); }
      else if (name == "speech_rate") speech_rate_ = param.as_double();
      else if (name == "loop_enabled") { loop_enabled_ = param.as_bool(); next_loop_ = now(); }
      else if (name == "messages") { messages_ = param.as_string_array(); loop_index_ = 0; completed_rounds_ = 0; next_loop_ = now(); }
      else if (name == "loop_message") { loop_message_ = param.as_string(); loop_index_ = 0; completed_rounds_ = 0; next_loop_ = now(); }
      else if (name == "interval_sec") { interval_sec_ = param.as_double(); next_loop_ = now() + rclcpp::Duration::from_seconds(interval_sec_); }
      else if (name == "repeat_count") { repeat_count_ = static_cast<int>(param.as_int()); completed_rounds_ = 0; }
      else if (name == "minimum_speech_guard_sec") guard_sec_ = param.as_double();
    }
    persist_config();
    publish_status();
    return result;
  }

  bool send_command(u_robot_audio_bridge::AudioCommand command, bool bypass_output_gate = false) {
    if (dry_run_ || (!output_enabled_ && !bypass_output_gate)) { last_return_code_ = 0; return true; }
    command.sequence = ++sequence_;
    std::strncpy(command.reply_path, reply_socket_.c_str(), sizeof(command.reply_path) - 1);
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (backend_socket_.size() >= sizeof(address.sun_path)) return false;
    std::memcpy(address.sun_path, backend_socket_.c_str(), backend_socket_.size() + 1);
    const auto sent = ::sendto(socket_fd_, &command, sizeof(command), 0,
      reinterpret_cast<sockaddr *>(&address), sizeof(address));
    if (sent != static_cast<ssize_t>(sizeof(command))) {
      last_error_ = "audio backend unavailable";
      last_return_code_ = -1;
      return false;
    }
    return true;
  }

  void request_speak(const std::string & text, const std::string & reason) {
    if (text.empty()) { last_error_ = "empty text rejected"; publish_status(); return; }
    if (text.size() > static_cast<std::size_t>(max_text_bytes_) || text.size() >= u_robot_audio_bridge::kMaxTextBytes) {
      last_error_ = "text exceeds max_text_bytes"; publish_status(); return;
    }
    const auto current = now();
    if (reason != "manual" && current < next_allowed_speak_) return;
    u_robot_audio_bridge::AudioCommand command{};
    command.type = u_robot_audio_bridge::AudioCommandType::speak;
    command.speaker_id = static_cast<std::uint16_t>(speaker_id_);
    std::memcpy(command.text, text.data(), text.size());
    if (send_command(command)) {
      current_text_ = text;
      last_reason_ = reason;
      last_error_.clear();
      next_allowed_speak_ = current + rclcpp::Duration::from_seconds(guard_sec_);
    }
    publish_status();
  }

  void send_volume() {
    u_robot_audio_bridge::AudioCommand command{};
    command.type = u_robot_audio_bridge::AudioCommandType::set_volume;
    command.volume = static_cast<std::uint8_t>(volume_);
    (void)send_command(command);
  }

  void stop() {
    loop_enabled_ = false;
    set_parameter(rclcpp::Parameter("loop_enabled", false));
    u_robot_audio_bridge::AudioCommand command{};
    command.type = u_robot_audio_bridge::AudioCommandType::stop;
    (void)send_command(command);
    current_text_.clear();
    last_reason_ = "stop";
    publish_status();
  }

  void receive_replies() {
    u_robot_audio_bridge::AudioReply reply{};
    while (::recv(socket_fd_, &reply, sizeof(reply), MSG_DONTWAIT) == static_cast<ssize_t>(sizeof(reply))) {
      if (reply.magic != u_robot_audio_bridge::kAudioIpcMagic) continue;
      last_return_code_ = reply.return_code;
      if (reply.return_code != 0) last_error_ = "SDK returned " + std::to_string(reply.return_code);
      else last_error_.clear();
      publish_status();
    }
  }

  void tick() {
    receive_replies();
    if (!loop_enabled_ || loop_message_.empty() || now() < next_loop_) return;
    if (repeat_count_ > 0 && completed_rounds_ >= repeat_count_) {
      loop_enabled_ = false;
      set_parameter(rclcpp::Parameter("loop_enabled", false));
      publish_status();
      return;
    }
    request_speak(loop_message_, "loop");
    ++completed_rounds_;
    next_loop_ = now() + rclcpp::Duration::from_seconds(std::max(interval_sec_, guard_sec_));
  }

  static std::string yaml_string(const std::string & value) {
    std::string escaped;
    escaped.reserve(value.size() + 2);
    for (const char character : value) {
      if (character == '\\' || character == '"') escaped.push_back('\\');
      if (character == '\n' || character == '\r') escaped += "\\n";
      else escaped.push_back(character);
    }
    return "\"" + escaped + "\"";
  }

  void persist_config() {
    if (persistent_config_file_.empty()) return;
    std::ofstream output(persistent_config_file_, std::ios::trunc);
    if (!output) {
      last_error_ = "cannot persist config: " + persistent_config_file_;
      RCLCPP_ERROR(get_logger(), "%s", last_error_.c_str());
      return;
    }
    output << "audio_bridge:\n  ros__parameters:\n"
           << "    dry_run: " << (dry_run_ ? "true" : "false") << '\n'
           << "    output_enabled_on_start: false\n"
           << "    speaker_id: " << speaker_id_ << '\n'
           << "    volume: " << volume_ << '\n'
           << "    speech_rate: 1.0\n"
           << "    loop_enabled: " << (loop_enabled_ ? "true" : "false") << '\n'
           << "    loop_message: " << yaml_string(loop_message_) << '\n'
           << "    messages: [" << yaml_string(loop_message_) << "]\n"
           << "    interval_sec: " << std::fixed << std::setprecision(3)
           << interval_sec_ << '\n'
           << "    repeat_count: " << repeat_count_ << '\n'
           << "    minimum_speech_guard_sec: " << std::fixed << std::setprecision(3)
           << guard_sec_ << '\n'
           << "    max_text_bytes: " << max_text_bytes_ << '\n'
           << "    backend_socket: " << yaml_string(backend_socket_) << '\n'
           << "    reply_socket: " << yaml_string(reply_socket_) << '\n';
    RCLCPP_INFO(get_logger(), "persisted audio parameters to %s", persistent_config_file_.c_str());
  }

  std::string status_json() const {
    std::ostringstream out;
    out << "{\"enabled\":" << (output_enabled_ ? "true" : "false")
        << ",\"dry_run\":" << (dry_run_ ? "true" : "false")
        << ",\"mode\":\"" << (loop_enabled_ ? "loop" : "idle") << "\""
        << ",\"volume\":" << volume_ << ",\"speaker_id\":" << speaker_id_
        << ",\"speech_rate\":" << std::fixed << std::setprecision(1) << speech_rate_
        << ",\"sdk_return_code\":" << last_return_code_
        << ",\"last_reason\":\"" << last_reason_ << "\""
        << ",\"current_text\":\"" << current_text_ << "\""
        << ",\"error\":\"" << last_error_ << "\"}";
    return out.str();
  }

  void publish_status() {
    std_msgs::msg::String msg; msg.data = status_json(); status_pub_->publish(msg);
  }

  void publish_diagnostics() {
    diagnostic_msgs::msg::DiagnosticArray array;
    array.header.stamp = now();
    diagnostic_msgs::msg::DiagnosticStatus status;
    status.name = "u_robot/audio_bridge";
    status.hardware_id = "unitree_a2_audio";
    status.level = last_error_.empty() ? diagnostic_msgs::msg::DiagnosticStatus::OK : diagnostic_msgs::msg::DiagnosticStatus::ERROR;
    status.message = last_error_.empty() ? (dry_run_ ? "dry-run" : "ready") : last_error_;
    diagnostic_msgs::msg::KeyValue value; value.key = "state"; value.value = status_json(); status.values.push_back(value);
    array.status.push_back(status); diagnostics_pub_->publish(array);
  }

  bool dry_run_{true}, output_enabled_{false}, loop_enabled_{false};
  int speaker_id_{0}, volume_{80}, repeat_count_{0}, completed_rounds_{0}, max_text_bytes_{500};
  double speech_rate_{1.0}, interval_sec_{10.0}, guard_sec_{4.0};
  std::vector<std::string> messages_;
  std::size_t loop_index_{0};
  std::string backend_socket_, reply_socket_, persistent_config_file_, loop_message_;
  std::string current_text_, last_reason_{"startup"}, last_error_;
  int socket_fd_{-1}, last_return_code_{0};
  std::uint32_t sequence_{0};
  rclcpp::Time next_loop_{0, 0, RCL_ROS_TIME}, next_allowed_speak_{0, 0, RCL_ROS_TIME};
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diagnostics_pub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr speak_sub_;
  rclcpp::Subscription<std_msgs::msg::Empty>::SharedPtr stop_sub_;
  rclcpp::Service<std_srvs::srv::SetBool>::SharedPtr enable_srv_;
  rclcpp::Service<std_srvs::srv::SetBool>::SharedPtr voice_service_srv_;
  OnSetParametersCallbackHandle::SharedPtr parameter_callback_;
  rclcpp::TimerBase::SharedPtr timer_, diagnostics_timer_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  try { rclcpp::spin(std::make_shared<AudioBridgeNode>()); }
  catch (const std::exception & error) { RCLCPP_FATAL(rclcpp::get_logger("audio_bridge"), "%s", error.what()); }
  rclcpp::shutdown();
  return 0;
}
