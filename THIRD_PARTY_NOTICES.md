# 第三方依赖与授权说明

## 本仓库状态

本次为私有审阅发布，未在整理过程中新增仓库级开源许可证。原始 `src/u_robot_audio_bridge/package.xml` 中的 `Apache-2.0` 声明保持原样；其来源与仓库级授权范围仍需权利人确认，不能把本说明理解为新增、撤销或扩张已有授权。公开发布及后续分发前应明确原创代码的授权范围。

## Unitree SDK2

音频源码依赖设备交付版 SDK，未把第三方 SDK 源码、静态库或动态库复制到此仓库。检查的来源工作区中：

- Git remote：`https://github.com/unitreerobotics/unitree_sdk2.git`。
- HEAD：`9754cd153af3da471b0fe5f3aa535e426fb11db3`。
- 开发容器中该 SDK 的 `git status --short` 为空。
- SDK 根 `LICENSE` 标为 BSD 3-Clause，版权归 HangZhou YuShu TECHNOLOGY CO., LTD.（Unitree Robotics）。

2026-09-13 在来源容器中只读核对的静态库 SHA256：

| 文件 | SHA256 |
|---|---|
| `lib/aarch64/libunitree_sdk2.a` | `a084cc0087b6dc1b6361f874aabed8bd525f0574b3b2228f91b4b512e1bf035e` |
| `lib/x86_64/libunitree_sdk2.a` | `08402aea74150dfbfc3fbfded4ca746916a8d892b54d2bade0cbf392a3be4029` |

这些值标识本次检查使用的依赖，不表示任意上游最新版本都兼容。优先保留与设备交付环境对应的完整 SDK，再核对上述版本与库文件。获取、布局和依赖准备也见 [宇树机器人容器化开发环境](https://github.com/lsclsc2026/unitree_docker)。

SDK 还随附 CycloneDDS、CycloneDDS C++、iceoryx 和 RapidJSON 的单独许可文本，位于其 `licenses/` 子目录；不能仅用 SDK 根许可证概括所有传递依赖。再分发包含这些依赖的二进制或镜像时，须保留并遵守对应版权和许可条件。

## ROS 与系统依赖

ROS 2 Humble、ament、rclcpp、ROS 消息/服务、launch、colcon，以及 C++ 标准库和系统库均为外部依赖。它们的许可证由各上游发行包提供；本仓库不重新授权这些组件。原始包依赖声明见 [package.xml](src/u_robot_audio_bridge/package.xml)。

本仓库的首版来源是已有开发容器中的独立音频工作区，旧交接说明已按源码核对后重写。没有导入个人云端地址、凭据、模型权重或第三方视频。
