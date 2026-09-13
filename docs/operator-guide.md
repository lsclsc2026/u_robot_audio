# 操作与调试

所有命令从 `u_robot_audio` 根目录执行，先完成[构建](installation.md)。本页包括会实际发声、改音量或切换机器人服务的操作；发布整理没有执行这些操作。

## 1. 不连接 SDK 的 ROS 演练

终端 A：

```bash
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 run u_robot_audio_bridge audio_bridge_node --ros-args   -p dry_run:=true -p output_enabled_on_start:=false -p loop_enabled:=false
```

终端 B（所有控制脚本自行加载 Humble 和当前工作区）：

```bash
./scripts/audio_ctl.sh say 这是一条演练文字
./scripts/audio_ctl.sh status
./scripts/audio_ctl.sh params
./scripts/audio_ctl.sh stop
```

这里没有启动 SDK 后端，且 `persistent_config_file` 内置默认值为空，因此不会写回仓库 YAML。状态中的 `current_text` 和 `sdk_return_code=0` 只是节点本地处理结果，不能证明实际播报。完成后在终端 A 按 Ctrl+C。

## 2. 连接机器人与单次发声

确认已结束演练实例，配置正确网卡与 SDK domain，再启动双进程：

```bash
./scripts/launch_audio.sh network_interface:=eth0 native_dds_domain_id:=0
```

替换示例网卡为自己的设备网卡。当前 YAML 已是 `dry_run=false`，但输出在启动时关闭。默认循环开关为 true，**先暂停循环再启用输出**。终端 B：

```bash
./scripts/audio_ctl.sh loop off
./scripts/audio_ctl.sh volume 60
./scripts/audio_ctl.sh voice-service on
./scripts/audio_ctl.sh enable
./scripts/audio_ctl.sh say 语音播报测试
./scripts/audio_ctl.sh status
```

`voice-service on` 会调用机器人 `vui_service` 开关；它绕过真实输出门控，但仍受 `dry_run` 控制。`enable` 本身只打开输出门控并发送音量设置，不自动启用 `vui_service`。观察后端 `ServiceSwitch`、`SetVolume` 和 `TtsMaker` 返回码，并由操作者确认实际声音。

若用自定义配置把 `dry_run` 设为 true，须改回 false 并重启节点；运行时 `ros2 param set dry_run false` 不会改变内部开关。

## 3. 循环播报

一次配置并开启循环：

```bash
./scripts/audio_ctl.sh configure --volume 60 --text '请注意安全' --interval 8
```

该命令检查音量范围、非空文字和间隔；间隔不能小于当前 `minimum_speech_guard_sec`。随后暂停循环、设置音量/文字/间隔、请求开启 `vui_service`、开启输出、再开启循环。它会实际改变设备状态，不是仅写配置。脚本根据 CLI 退出码执行，不能据其最后一句“已启动”判断 SDK 或实际播放成功，应继续查看状态和后端日志。

也可以逐项调整：

```bash
./scripts/audio_ctl.sh loop off
./scripts/audio_ctl.sh volume 60
./scripts/audio_ctl.sh message '前方通道请保持畅通'
./scripts/audio_ctl.sh interval 12
./scripts/audio_ctl.sh loop on
```

循环内容来自单条 `loop_message`，不是 `messages` 数组。`message` 和 `messages` 两个快捷命令名都设置 `loop_message`。`say` 与 `message` 不接受英文单引号；可用 ROS 参数或支持正确 JSON 编码的 Foxglove 面板输入，注意状态文本目前没有完整 JSON 转义。

限定调度次数时，先加载环境再修改参数：

```bash
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 param set /audio_bridge repeat_count 3
```

次数记录调度尝试，不是声音完成次数。再次开启已达到次数上限的循环前，可重新设置 `repeat_count` 或文字以重置计数；设为 `0` 恢复无限循环。

## 4. 停止、暂停与退出

```bash
./scripts/audio_ctl.sh stop
./scripts/audio_ctl.sh disable
./scripts/audio_ctl.sh status
```

`stop` 关闭循环，并在真实输出仍启用、非 dry-run 时尝试发送 `PlayStop`。`disable` 关闭真实输出门控和循环，但其内部 stop 调用发生在关闭门控之后，不能替代先执行 `stop`。`loop off` 仅停止后续循环，不请求中断当前声音。原生 TTS 是否被 `PlayStop` 立即打断还需固件实测。

若需要关闭机器人语音服务，可再运行 `./scripts/audio_ctl.sh voice-service off`；这是改变设备服务状态，不是关闭当前 ROS 节点。结束前根据现场其他应用是否使用该服务决定是否执行。最后在 launch 终端按 Ctrl+C；仅退出节点不保证设备声音停止。

## 5. 配置保存

运行时参数修改通常会自动覆盖安装目录中的默认 YAML；`--symlink-install` 下可能回写到源码配置。停止也会保存 `loop_enabled=false`，因此下次启动的默认循环状态可能与首次克隆不同。`output_enabled_on_start` 保存时强制为 false。

即使 launch 使用 `parameters_file:=/path/to/custom.yaml`，当前实现仍把运行时修改写入包安装目录的默认 YAML，而不是该自定义文件。部署前备份自己的配置，并在版本提交前检查差异。完整规则见[配置文档](configuration.md)。

## 6. Foxglove

本仓库不启动 Foxglove Bridge；可复用 [u_robot_move](https://github.com/lsclsc2026/u_robot_move) 中配置好的桥接。Publish 面板向 `/operator/audio/speak` 发布 `std_msgs/msg/String`，例如 `{ "data": "播报测试" }`；向 `/operator/audio/stop` 发布 `std_msgs/msg/Empty` 即请求停止。Parameters 面板可修改 `loop_message`、`volume`、`interval_sec`、`loop_enabled` 等运行时参数；服务调用面板可调用两个 SetBool 服务。

参数面板能否显示节点取决于桥接是否开放参数能力；使用 CLI 与面板发送真实命令具有相同效果。

## 7. 直接 SDK 调试工具

这些程序绕过 ROS 节点的 dry-run、输出开关和文本上限。SDK 库路径与 DDS 网络必须预先准备；不要把可执行文件名中的 `probe` 当作无副作用保证。

| 程序/脚本 | 参数和实际行为 |
|---|---|
| `scripts/test_tts.sh` | `--volume N --text 文字`，可选 `--speaker 0或1 --interface 网卡 --domain 域`；自动设置库路径并调用 TTS probe。无参数也会按音量 60、中文、eth0、domain 0 发声，不是帮助页 |
| `a2_tts_probe` | `<interface> <domain> <volume> <speaker_id> <text>`；开启 `vui_service`、设置/查询音量、TTS，等待 10 秒；不自动关服务或恢复音量 |
| `a2_voice_service_probe` | `[interface] [domain]`，默认 eth0/0；查询服务列表和音量，不执行服务开关，但会连接机器人；退出码只反映服务列表查询结果 |
| `a2_pcm_probe` | `<interface> <domain> [volume]`，默认音量 60；设置音量、发送 3 秒 700 Hz 的 16 kHz/单声道/s16le PCM，等待后停止；不是文件播放器 |

直接 TTS 示例（**会发声**）：

```bash
./scripts/test_tts.sh --volume 60 --speaker 0 --interface eth0 --domain 0 --text '语音播报测试'
```

这些工具用于区分 ROS 桥接问题与 SDK/固件问题，不能代替实际试听或整套验收。
