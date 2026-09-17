# 宇树机器人语音播报系统

**Unitree A2 Pro ROS 2 text-to-speech bridge** · 独立的文字播报与定时循环播报工作区。

通过 ROS 2 话题、服务和参数控制 A2 原生 TTS，由独立 SDK 进程连接机器人。可以单次播报、设置一条循环文字、调整音量和间隔、切换机器人语音服务，并查看 SDK 返回码。源码采用 ROS 2 Humble、C++17 和 Unitree SDK2。

本次为私有首版审阅整理。已实现功能依据源码确认，发布检查与实机验收分别记录在[验证记录](docs/validation.md)。没有实现语音识别、自由对话、自定义音色或可调语速；`speech_rate` 目前只支持 `1.0`。

## 快速开始

在配置好 ROS 2 Humble 与设备交付版 SDK 的 Linux 环境中，采用以下并列布局：

```text
workspace/
├── sdk/unitree_sdk2/
├── u_robot_audio/
├── u_robot_move/          # 可选：导航
├── u_robot_duck_dataset/  # 可选：视觉采集
└── unitree_docker/        # 共用环境
```

从 `u_robot_audio` 根目录构建：

```bash
./scripts/build.sh
```

安装依赖与 SDK 校验见[安装文档](docs/installation.md)。只检查 ROS 接口、完全不启动 SDK 后端的演练：

```bash
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 run u_robot_audio_bridge audio_bridge_node --ros-args   -p dry_run:=true -p output_enabled_on_start:=false -p loop_enabled:=false
```

另一个已加载相同 ROS 环境的终端可运行 `./scripts/audio_ctl.sh status` 和 `./scripts/audio_ctl.sh params`。演练状态不代表机器人接受了 RPC 或实际发出了声音。

实际连接和发声按[操作指南](docs/operator-guide.md)执行。**随仓库 YAML 的默认值为 `dry_run: false`、`loop_enabled: true`、`volume: 100`，但 `output_enabled_on_start: false`。** 启用真实输出前先关闭循环并选择音量；`configure` 会开启真实输出和 `vui_service` 并启动循环。

## 文档

| 文档 | 内容 |
|---|---|
| [安装与 SDK](docs/installation.md) | 环境、共享依赖、编译和库路径 |
| [架构](docs/architecture.md) | 双进程、IPC、调度与反馈 |
| [操作指南](docs/operator-guide.md) | 演练、单次/循环、停止、调试工具 |
| [接口与参数](docs/configuration.md) | ROS API、默认值、运行时修改与持久化 |
| [排障与限制](docs/troubleshooting.md) | DDS、无声音、状态和已知限制 |
| [验证记录](docs/validation.md) | 实际检查范围与待实机项目 |
| [第三方与授权说明](THIRD_PARTY_NOTICES.md) | SDK 来源、依赖许可证、私有审阅说明 |
| [版本记录](CHANGELOG.md) | 首版整理范围 |

## 相关项目

- [宇树四足机器人室内导航与多点巡逻](https://github.com/lsclsc2026/u_robot_move)：建图、定位、导航与多点巡逻。
- [宇树机器人语音播报系统](https://github.com/lsclsc2026/u_robot_audio)：本语音模块。
- [机器人视觉数据采集与鸭子检测工具](https://github.com/lsclsc2026/u_robot_duck_dataset)：鸭子数据采集与自动标注研究工具。
- [宇树机器人容器化开发环境](https://github.com/lsclsc2026/unitree_docker)：共享开发环境与部署入口。

这些仓库首版为私有，访问链接需要相应权限。音频工作区不依赖导航工作区启动，也不发布运动指令。
