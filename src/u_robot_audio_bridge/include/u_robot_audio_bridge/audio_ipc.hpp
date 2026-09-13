#pragma once

#include <cstdint>

namespace u_robot_audio_bridge {

constexpr std::uint32_t kAudioIpcMagic = 0x41324155U;
constexpr std::size_t kMaxTextBytes = 1024U;

enum class AudioCommandType : std::uint8_t {
  speak = 1, stop = 2, set_volume = 3, voice_service_switch = 4
};

struct AudioCommand {
  std::uint32_t magic{kAudioIpcMagic};
  std::uint32_t sequence{0};
  AudioCommandType type{AudioCommandType::stop};
  std::uint8_t volume{80};
  std::uint16_t speaker_id{0};
  char reply_path[108]{};
  char text[kMaxTextBytes]{};
};

struct AudioReply {
  std::uint32_t magic{kAudioIpcMagic};
  std::uint32_t sequence{0};
  AudioCommandType type{AudioCommandType::stop};
  std::int32_t return_code{0};
};

}  // namespace u_robot_audio_bridge
