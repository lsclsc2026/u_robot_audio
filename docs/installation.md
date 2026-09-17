# 安装与构建

## 环境与布局

目标环境为 Linux、ROS 2 Humble、C++17、CMake 3.8 以上、colcon，以及与 A2 设备匹配的 Unitree SDK2。本次发布与隔离构建针对 x86_64；源码和 SDK 另有 aarch64 库选择分支，但尚未完成该架构的完整构建或实机验证。实际构建结果见[验证记录](validation.md)，共用容器材料见 [宇树机器人容器化开发环境](https://github.com/lsclsc2026/unitree_docker)。

ROS 环境应提供 `ament_cmake`、`rclcpp`、`diagnostic_msgs`、`std_msgs`、`std_srvs`、`ament_index_python`、`launch` 和 `launch_ros`。节点还直接使用 `rcl_interfaces` 类型，通常由 `rclcpp` 的依赖链提供。发布版保留原有包声明，构建结果单独记录。

若已安装 ROS 2 Humble 并配置相应 apt 软件源，可安装缺少的构建及消息包：

```bash
sudo apt-get update
sudo apt-get install build-essential cmake python3-colcon-common-extensions   ros-humble-ament-cmake ros-humble-rclcpp ros-humble-rcl-interfaces   ros-humble-diagnostic-msgs ros-humble-std-msgs ros-humble-std-srvs   ros-humble-ament-index-python ros-humble-launch ros-humble-launch-ros
```

在自己的工作目录克隆私有仓库，需先具备读取权限：

```bash
git clone https://github.com/lsclsc2026/u_robot_audio.git
```

把匹配的 SDK 放到 `u_robot_audio/../sdk/unitree_sdk2/`。SDK 不打包进本仓库，来源与校验值见[第三方说明](../THIRD_PARTY_NOTICES.md)。不要用系统里任意 `/opt` 安装的旧版库替换；构建脚本明确使用并列 SDK 的静态库，以满足 A2 `TtsMaker` 需要的符号。

```text
sdk/unitree_sdk2/
├── include/unitree/robot/a2/audio/audio_client.hpp
├── lib/aarch64/libunitree_sdk2.a
├── lib/x86_64/libunitree_sdk2.a
└── thirdparty/
    ├── include/
    └── lib/{aarch64,x86_64}/
```

## 构建

以下命令从仓库根目录运行；构建不启动音频程序：

```bash
./scripts/build.sh
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 pkg prefix u_robot_audio_bridge
ros2 pkg executables u_robot_audio_bridge
```

预期安装五个程序：`audio_bridge_node`、`a2_audio_backend`、`a2_voice_service_probe`、`a2_tts_probe`、`a2_pcm_probe`。这里只查询可执行文件列表，不运行 probe。脚本默认使用 `colcon build --symlink-install --base-paths src`。

## SDK 库与网络

CMake 固定从并列 `../sdk/unitree_sdk2` 选择库，`scripts/test_tts.sh` 使用相同位置。launch 根据 colcon 安装前缀推导该目录，支持默认独立安装和 `--merge-install`；如果仅运行时库位置不同，可在 launch 前设置 `UNITREE_SDK2_ROOT`，该变量不会改变 CMake 或直接 TTS 脚本的依赖位置。

```bash
export UNITREE_SDK2_ROOT=/path/to/sdk/unitree_sdk2
```

`network_interface` 默认 `eth0`，应替换为能与机器人 DDS 通信的网卡；`native_dds_domain_id` 默认 `0`。ROS 2 客户端终端与 ROS 节点的 `ROS_DOMAIN_ID` 应一致，它不自动改变 SDK 的 domain。共享主机网络、DDS 配置与容器挂载方式见 Docker 仓库。不要同时启动多个使用默认节点名和 `/tmp` socket 路径的实例。

接下来先按[操作指南](operator-guide.md)运行不带 SDK 的 dry-run，再由操作者进行实机播报。首版整理的验证边界见[验证记录](validation.md)。
