#include <unitree/robot/a2/audio/audio_client.hpp>
#include <unitree/robot/channel/channel_factory.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char ** argv) {
  if (argc < 3) {
    std::cerr << "Usage: a2_pcm_probe <interface> <domain> [volume]\n";
    return 2;
  }
  const std::string interface_name = argv[1];
  const int domain = std::stoi(argv[2]);
  const int volume_value = argc >= 4 ? std::stoi(argv[3]) : 60;
  if (volume_value < 0 || volume_value > 100) {
    std::cerr << "volume must be 0..100\n";
    return 2;
  }

  unitree::robot::ChannelFactory::Instance()->Init(domain, interface_name);
  unitree::robot::a2::AudioClient client;
  client.SetTimeout(10.0F);
  client.Init();

  const int32_t volume_result = client.SetVolume(static_cast<uint8_t>(volume_value));
  std::cout << "SetVolume ret=" << volume_result << '\n';

  constexpr int sample_rate = 16000;
  constexpr int duration_seconds = 3;
  constexpr double frequency_hz = 700.0;
  constexpr double amplitude = 10000.0;
  constexpr double pi = 3.14159265358979323846;
  std::vector<uint8_t> pcm;
  pcm.reserve(sample_rate * duration_seconds * 2);
  for (int i = 0; i < sample_rate * duration_seconds; ++i) {
    const double fade = std::min({1.0, i / 800.0,
      (sample_rate * duration_seconds - 1 - i) / 800.0});
    const auto sample = static_cast<int16_t>(
      amplitude * fade * std::sin(2.0 * pi * frequency_hz * i / sample_rate));
    pcm.push_back(static_cast<uint8_t>(sample & 0xff));
    pcm.push_back(static_cast<uint8_t>((static_cast<uint16_t>(sample) >> 8) & 0xff));
  }

  const std::string app_name = "u_robot_audio_pcm_probe";
  const std::string stream_id = "pcm_probe_700hz";
  const int32_t play_result = client.PlayStream(app_name, stream_id, pcm);
  std::cout << "PlayStream ret=" << play_result
            << ", bytes=" << pcm.size() << ", format=16000Hz/mono/s16le"
            << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(duration_seconds + 2));
  client.PlayStop(app_name);
  std::cout << "PlayStop sent" << std::endl;
  return volume_result == 0 && play_result == 0 ? 0 : 1;
}
