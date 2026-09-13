# u_robot_audio_bridge

ROS 2 Humble 的 A2 原生文字播报适配器，由 ROS 节点、SDK 后端和三个诊断程序组成。

本包从独立 `u_robot_audio` 工作区构建。完整说明以[工作区 README](../../README.md)、[接口与参数](../../docs/configuration.md)及[操作指南](../../docs/operator-guide.md)为准。

注意区分节点内置默认值与 launch 加载的 YAML：随仓库 YAML 为 `dry_run=false`、`volume=100`、`loop_enabled=true`，真实输出在启动时关闭。不要把本包描述为具备语音识别、对话、可调音色或任意语速的语音系统。
