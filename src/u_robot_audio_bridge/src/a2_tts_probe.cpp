#include <unitree/robot/a2/audio/audio_client.hpp>
#include <unitree/robot/channel/channel_factory.hpp>
#include <unitree/robot/go2/robot_state/robot_state_client.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char ** argv) {
  if (argc < 6) {
    std::cerr << "Usage: a2_tts_probe <interface> <domain> <volume> <speaker_id> <text>\n";
    return 2;
  }
  const std::string interface_name = argv[1];
  const int domain = std::stoi(argv[2]);
  const int volume_value = std::stoi(argv[3]);
  const int speaker_id = std::stoi(argv[4]);
  const std::string text = argv[5];
  if (volume_value < 0 || volume_value > 100 || (speaker_id != 0 && speaker_id != 1)) {
    std::cerr << "volume must be 0..100 and speaker_id must be 0 or 1\n";
    return 2;
  }
  unitree::robot::ChannelFactory::Instance()->Init(domain, interface_name);
  unitree::robot::go2::RobotStateClient robot_state_client;
  robot_state_client.SetTimeout(5.0F);
  robot_state_client.Init();
  int32_t service_status = 0;
  const int32_t service_result =
    robot_state_client.ServiceSwitch("vui_service", 1, service_status);
  std::cout << "Enable vui_service ret=" << service_result
            << ", status=" << service_status << '\n';

  unitree::robot::a2::AudioClient client;
  client.SetTimeout(5.0F);
  client.Init();
  const int32_t volume_result = client.SetVolume(static_cast<uint8_t>(volume_value));
  std::cout << "SetVolume ret=" << volume_result << '\n';
  uint8_t actual_volume = 0;
  const int32_t get_volume_result = client.GetVolume(actual_volume);
  std::cout << "GetVolume ret=" << get_volume_result
            << ", volume=" << static_cast<int>(actual_volume) << '\n';
  const int32_t tts_result = client.TtsMaker(text, speaker_id);
  std::cout << "TtsMaker ret=" << tts_result << '\n';
  std::cout << "Waiting 10 seconds for asynchronous playback..." << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(10));
  return service_result == 0 && volume_result == 0 && get_volume_result == 0 &&
      tts_result == 0 ? 0 : 1;
}
