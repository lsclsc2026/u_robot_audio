#include <unitree/robot/channel/channel_factory.hpp>
#include <unitree/robot/a2/audio/audio_client.hpp>
#include <unitree/robot/go2/robot_state/robot_state_client.hpp>

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char ** argv) {
  const std::string interface_name = argc > 1 ? argv[1] : "eth0";
  const int domain = argc > 2 ? std::stoi(argv[2]) : 0;
  unitree::robot::ChannelFactory::Instance()->Init(domain, interface_name);
  unitree::robot::go2::RobotStateClient client;
  client.SetTimeout(5.0F);
  client.Init();
  std::vector<unitree::robot::go2::ServiceState> services;
  const int32_t result = client.ServiceList(services);
  std::cout << "ServiceList ret=" << result << ", count=" << services.size() << '\n';
  for (const auto & service : services) {
    std::cout << service.name << " status=" << service.status
              << " protect=" << service.protect << '\n';
  }
  unitree::robot::a2::AudioClient audio_client;
  audio_client.SetTimeout(5.0F);
  audio_client.Init();
  uint8_t volume = 0;
  const int32_t volume_result = audio_client.GetVolume(volume);
  std::cout << "GetVolume ret=" << volume_result;
  if (volume_result == 0) std::cout << ", volume=" << static_cast<int>(volume);
  std::cout << '\n';
  return result == 0 ? 0 : 1;
}
