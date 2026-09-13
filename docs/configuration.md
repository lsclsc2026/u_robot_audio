# ROS 接口与配置

## 话题与服务

名称在源码中使用绝对路径，快捷脚本也固定使用 `/audio_bridge`；通过命名空间启动不会自动形成互不干扰的多实例系统。

| 名称 | 类型 | 方向与含义 |
|---|---|---|
| `/operator/audio/speak` | `std_msgs/msg/String` | 输入：`data` 为单次播报文字；手动请求绕过循环最短保护间隔 |
| `/operator/audio/stop` | `std_msgs/msg/Empty` | 输入：关闭循环并尝试停止播放 |
| `/audio/status` | `std_msgs/msg/String` | 输出：状态字符串，reliable、transient_local、KeepLast(1) |
| `/diagnostics` | `diagnostic_msgs/msg/DiagnosticArray` | 输出：每 2 秒发布 `u_robot/audio_bridge` 状态，队列深度 10 |
| `/audio_bridge/enable_output` | `std_srvs/srv/SetBool` | `true` 开门并设置音量；`false` 关门并关闭循环；success 只表示节点接受操作 |
| `/audio_bridge/enable_voice_service` | `std_srvs/srv/SetBool` | 请求机器人 `vui_service` 开/关，绕过输出门控，受 dry-run 控制；success 不等于 SDK 已完成 |

直接 ROS 命令示例（需先加载 ROS 与工作区环境；开启真实输出后会影响设备）：

```bash
ros2 topic pub --once /operator/audio/speak std_msgs/msg/String "{data: '播报测试'}"
ros2 topic pub --once /operator/audio/stop std_msgs/msg/Empty '{}'
ros2 service call /audio_bridge/enable_output std_srvs/srv/SetBool '{data: true}'
ros2 topic echo --once /audio/status
ros2 param dump /audio_bridge
```

## 默认值与运行时参数

“YAML”指 [audio_bridge.yaml](../src/u_robot_audio_bridge/config/audio_bridge.yaml)，默认 launch 会加载它；“内置”指不指定 YAML、直接运行 `audio_bridge_node`。启动覆盖值的校验不等同于运行时校验，不应依赖节点拒绝所有不合法的初始配置。

| 参数 | YAML / 内置默认值 | 运行时行为与范围 |
|---|---|---|
| `dry_run` | false / true | 仅启动时读取；true 时不发送任何 SDK 命令 |
| `output_enabled_on_start` | false / false | 仅启动时读取；运行中用 enable_output 服务控制实际开关 |
| `speaker_id` | 0 / 0 | 可改；0 中文、1 英文，不是任意音色选择 |
| `volume` | 100 / 80 | 可改；整数 0–100；输出关闭时只更新本地值，启用时再下发 |
| `speech_rate` | 1.0 / 1.0 | 运行时只接受接近 1.0 的值（容差 0.001）；SDK 不接收语速字段 |
| `loop_enabled` | true / false | 可改；改为 true 后下一次调度可立即开始，仍受输出门控与保护间隔影响 |
| `loop_message` | “导航进行中，请让我先走，谢谢！” / 第一条 messages | 可改；实际单条循环内容；空字符串暂停产生循环请求 |
| `messages` | 含默认循环文字的单元素数组 / `[大家好，我是宇树机器人。]` | 保留的数组参数；修改会重置调度计数，但不替换当前 loop_message，也不实现数组轮播 |
| `interval_sec` | 8.0 / 10.0 | 可改；至少 0.5 秒，实际循环周期下限为保护间隔 |
| `repeat_count` | 0 / 0 | 可改；非负整数，0 无限；按调度尝试次数计数 |
| `minimum_speech_guard_sec` | 4.0 / 4.0 | 可改；非负秒数，仅限制非手动播报，不等待播放完成 |
| `max_text_bytes` | 500 / 500 | 仅启动时读取；UTF-8 字节数，还受 IPC 小于 1024 字节限制 |
| `backend_socket` | `/tmp/u_robot_a2_audio.sock` | 仅启动时读取；应与后端 socket 路径一致 |
| `reply_socket` | `/tmp/u_robot_audio_bridge_reply.sock` | 仅启动时读取；单实例本地回复 socket |
| `persistent_config_file` | launch 指定包默认 YAML / 空字符串 | 仅启动时读取；空字符串禁用持久化 |

仅启动时读取的参数仍可能被 `ros2 param set` 接受并改变 ROS 参数存储值，但内部成员不会随之更新，`param get` 与实际行为可能不一致。更改这些参数请修改启动配置并重启。输出运行时状态以 `/audio/status.enabled` 为准。

运行时修改 `volume` 会尝试发命令，修改 `loop_message` 或 `messages` 会重置次数并将下一次循环设为现在；修改 `interval_sec` 会按新间隔重新安排下一次循环。改动前可先 `loop off`。

## Launch 参数

| 参数 | 默认值 | 作用 |
|---|---|---|
| `parameters_file` | 包 share 目录 `config/audio_bridge.yaml` | ROS 参数来源 |
| `network_interface` | `eth0` | Unitree 原生 DDS 网卡 |
| `native_dds_domain_id` | `0` | Unitree 原生 DDS 域，不等同于自动继承 `ROS_DOMAIN_ID` |
| `backend_socket` | `/tmp/u_robot_a2_audio.sock` | 同时覆盖 ROS 节点与 SDK 后端 socket 路径 |

例如 `./scripts/launch_audio.sh parameters_file:=/path/to/audio.yaml network_interface:=eth0`。`UNITREE_SDK2_ROOT` 是可选的运行时 SDK 根目录环境变量，详见[安装文档](installation.md)。

## 持久化规则

默认 launch 总把 `persistent_config_file` 设置为**安装目录中的默认配置文件**，即使读取了另一份 `parameters_file`。参数回调会用内部成员重写整个 YAML：输出启动值强制 false、语速写 1.0、`messages` 写成当前单条 `loop_message`。它不会保留自定义注释、未知字段或原来的多元素数组；没有原子替换与并发写保护。文件不可写时记录错误，但参数设置本身可能仍返回成功。

使用 `--symlink-install` 时，安装配置可能链接到源码，运行控制命令后应检查 `git diff`。若希望完全关闭写回，可直接运行节点并显式使用空的 `persistent_config_file`；默认 launch 没有单独暴露该参数覆盖入口。

## 状态字段与解释

`/audio/status` 中有 `enabled`、`dry_run`、`mode`（loop/idle）、`volume`、`speaker_id`、`speech_rate`、`sdk_return_code`、`last_reason`、`current_text` 和 `error`。诊断消息在 key 为 `state` 的项中重复同一字符串。

这些是节点最近一次请求/反馈，不是机器人完整播放状态：`mode=loop` 不表示正在发声，`enabled=true` 不表示 `vui_service` 已启用，返回码 0 在门控或 dry-run 下也会出现。后端回复没有超时关联处理，节点也未检查 sequence 对应关系；旧返回码可能暂时保留。

当前拼接状态字符串没有对引号、反斜线、换行做完整 JSON 转义。普通中文可读，但含特殊字符时不能保证可被 JSON 解析器解析；订阅类型始终是 `std_msgs/String`，客户端应保留原始字符串以便诊断。
