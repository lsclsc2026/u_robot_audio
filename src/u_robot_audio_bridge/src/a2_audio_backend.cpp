#include "u_robot_audio_bridge/audio_ipc.hpp"

#include <unitree/robot/a2/audio/audio_client.hpp>
#include <unitree/robot/channel/channel_factory.hpp>
#include <unitree/robot/go2/robot_state/robot_state_client.hpp>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <csignal>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::atomic_bool running{true};
void signal_handler(int) { running.store(false); }
}

int main(int argc, char ** argv) {
  std::string interface_name = "eth0";
  std::string socket_path = "/tmp/u_robot_a2_audio.sock";
  int domain = 0;
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string key{argv[i]};
    if (key == "--network-interface") interface_name = argv[i + 1];
    else if (key == "--socket-path") socket_path = argv[i + 1];
    else if (key == "--domain-id") domain = std::stoi(argv[i + 1]);
    else { std::cerr << "unknown option: " << key << '\n'; return 2; }
  }

  int fd = -1;
  try {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    ::unlink(socket_path.c_str());
    fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) throw std::runtime_error("socket creation failed");
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (socket_path.size() >= sizeof(address.sun_path)) throw std::runtime_error("socket path too long");
    std::memcpy(address.sun_path, socket_path.c_str(), socket_path.size() + 1);
    if (::bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
      throw std::runtime_error("socket bind failed");

    unitree::robot::ChannelFactory::Instance()->Init(domain, interface_name);
    unitree::robot::a2::AudioClient client;
    client.SetTimeout(3.0F);
    client.Init();
    unitree::robot::go2::RobotStateClient robot_state_client;
    robot_state_client.SetTimeout(5.0F);
    robot_state_client.Init();
    std::cout << "[a2_audio_backend] ready on " << interface_name << ", domain " << domain
              << "; waiting for robot voice RPC" << std::endl;

    while (running.load()) {
      pollfd pfd{fd, POLLIN, 0};
      const int ready = ::poll(&pfd, 1, 200);
      if (ready <= 0 || !(pfd.revents & POLLIN)) continue;
      u_robot_audio_bridge::AudioCommand command{};
      const auto count = ::recv(fd, &command, sizeof(command), 0);
      if (count != static_cast<ssize_t>(sizeof(command)) ||
          command.magic != u_robot_audio_bridge::kAudioIpcMagic) continue;
      command.text[sizeof(command.text) - 1] = '\0';
      command.reply_path[sizeof(command.reply_path) - 1] = '\0';
      int32_t result = -1;
      if (command.type == u_robot_audio_bridge::AudioCommandType::speak) {
        result = client.TtsMaker(command.text, command.speaker_id);
        std::cout << "[a2_audio_backend] TtsMaker speaker=" << command.speaker_id
                  << " bytes=" << std::strlen(command.text) << " ret=" << result << std::endl;
      } else if (command.type == u_robot_audio_bridge::AudioCommandType::stop) {
        result = client.PlayStop("u_robot_audio_bridge");
        std::cout << "[a2_audio_backend] PlayStop ret=" << result << std::endl;
      } else if (command.type == u_robot_audio_bridge::AudioCommandType::set_volume) {
        result = client.SetVolume(command.volume);
        std::cout << "[a2_audio_backend] SetVolume value=" << static_cast<int>(command.volume)
                  << " ret=" << result << std::endl;
      } else if (command.type == u_robot_audio_bridge::AudioCommandType::voice_service_switch) {
        int32_t service_status = 0;
        result = robot_state_client.ServiceSwitch(
          "vui_service", command.volume != 0 ? 1 : 0, service_status);
        std::cout << "[a2_audio_backend] ServiceSwitch vui_service enabled="
                  << (command.volume != 0 ? "true" : "false") << " ret=" << result
                  << " status=" << service_status << std::endl;
      }
      u_robot_audio_bridge::AudioReply reply{};
      reply.sequence = command.sequence;
      reply.type = command.type;
      reply.return_code = result;
      sockaddr_un reply_address{};
      reply_address.sun_family = AF_UNIX;
      std::memcpy(reply_address.sun_path, command.reply_path, std::strlen(command.reply_path) + 1);
      (void)::sendto(fd, &reply, sizeof(reply), 0,
        reinterpret_cast<sockaddr *>(&reply_address), sizeof(reply_address));
    }
    ::close(fd);
    ::unlink(socket_path.c_str());
    return 0;
  } catch (const std::exception & error) {
    std::cerr << "[a2_audio_backend] fatal: " << error.what() << std::endl;
    if (fd >= 0) ::close(fd);
    ::unlink(socket_path.c_str());
    return 1;
  }
}
