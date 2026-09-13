# 架构与行为

## 进程边界

```mermaid
flowchart LR
    CLI[ROS CLI / audio_ctl / Foxglove] -->|话题、参数、服务| ROS[audio_bridge_node]
    ROS -->|Unix datagram AudioCommand| SDK[a2_audio_backend]
    SDK -->|AudioReply 返回码| ROS
    SDK -->|Unitree SDK2 / 原生 DDS| ROBOT[A2 音频与 vui_service]
    ROS --> STATUS[/audio/status 与 /diagnostics]
```

`audio_bridge_node` 链接 ROS 2 库，负责输出开关、文本校验、循环调度、参数落盘和状态发布；`a2_audio_backend` 链接设备交付版 SDK 静态库和其 CycloneDDS 库，调用 `AudioClient` 与 `RobotStateClient`。两个进程通过本机 Unix 数据报套接字通信，避免在同一进程内组合 ROS 与 SDK 的 DDS 依赖。

默认 launch 同时启动两个进程；SDK 后端退出后由 launch 在 2 秒后重启。后端初始化时设置 RPC 超时：音频 3 秒、语音服务 5 秒。launch 中的 `network_interface` 和 `native_dds_domain_id` 只传给 SDK 后端。`scripts/launch_audio.sh` 在启动前取消继承的 `CYCLONEDDS_URI`，因此使用该脚本时也要核对 ROS RMW 的实际网络配置。

## 请求路径

1. `/operator/audio/speak` 接收 UTF-8 文本。空文本、超过 `max_text_bytes` 或达到 IPC 上限 1024 字节的文本被拒绝。
2. `dry_run=true` 或真实输出关闭时，普通命令不发送到 SDK，但节点仍可能更新文字、原因、调度计数和本地成功状态。
3. 实际请求通过 `AudioCommand` 发送，后端调用 SDK，再返回 `AudioReply`。
4. `/audio/status` 保存最后状态，`/diagnostics` 每 2 秒发布一次。本地提交成功、SDK 返回成功和听到声音是三个不同的检查点。

IPC 包定义见 [audio_ipc.hpp](../src/u_robot_audio_bridge/include/u_robot_audio_bridge/audio_ipc.hpp)。协议包含 magic、sequence、命令类型、音量、speaker、回复路径和文本；这是同机、同构建版本之间的原生结构体协议，没有跨平台序列化、身份认证或消息重试机制。两端应一起构建升级。

## 循环与停止

调度器每 100 ms 检查一次，实际播报一条 `loop_message`；`messages` 数组仍在参数接口中，但没有按数组轮播的实现。下一次循环间隔使用 `max(interval_sec, minimum_speech_guard_sec)`。手动播报绕过最短保护间隔；它可能打断或覆盖前次请求，最终表现由设备固件决定。

`repeat_count=0` 表示不限定次数；正值按循环调度尝试次数计数，不是根据音频播放完成回调计数。输出关闭、后端失败等情况下也不能把计数理解为实际成功播报次数。重新打开循环不会重置已完成次数，修改文字或 `repeat_count` 会重置。

停止话题先关闭循环，再尝试发送 `PlayStop("u_robot_audio_bridge")`。输出关闭或 dry-run 时请求仍被门控。关闭输出服务先关输出再调用停止，因此**要尝试中断当前播放，应先 `stop`，再 `disable`**。固件是否允许该应用名停止原生 TTS，需要实机验证。退出节点不保证停止机器人正在播放的声音。

## 源码目录

| 路径 | 职责 |
|---|---|
| `src/u_robot_audio_bridge/src/audio_bridge_node.cpp` | ROS 节点与参数持久化 |
| `src/u_robot_audio_bridge/src/a2_audio_backend.cpp` | SDK RPC 后端 |
| `src/u_robot_audio_bridge/src/a2_*_probe.cpp` | 服务查询、TTS、PCM 单独诊断工具 |
| `src/u_robot_audio_bridge/launch/audio_bridge.launch.py` | 双进程启动、SDK 库目录 |
| `src/u_robot_audio_bridge/config/audio_bridge.yaml` | 默认配置 |
| `scripts/` | 构建、launch、控制、直接 TTS 探测 |
