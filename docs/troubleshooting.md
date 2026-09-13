# 排障与已知限制

## 构建或加载 SDK 失败

- 找不到 `audio_client.hpp` 或 SDK 静态库：检查 `../sdk/unitree_sdk2` 布局和当前 CPU 对应的 `lib`、`thirdparty/lib` 目录。
- 出现 `uint16` JSON/TTS 未定义符号：CMake 注释记录系统 `/opt` 曾有不匹配旧库。本项目显式链接设备交付版归档；对照[SDK 校验值](../THIRD_PARTY_NOTICES.md)，不要混用不同版本头文件和库。
- 找不到 `libddsc` / `libddscxx`：检查 SDK `thirdparty/lib`、CPU 架构和 launch 推导出的 SDK 路径。自定义安装位置可设置 `UNITREE_SDK2_ROOT`；这只影响 launch 的运行时目录。
- `ros2` 或 install setup 不存在：先加载 `/opt/ros/humble/setup.bash` 并完成构建。`audio_ctl.sh help` 本身也会先加载工作区，所以不能作为“未安装也能运行”的检测命令。

## 找不到节点或 IPC 后端

使用同一 ROS 环境和 `ROS_DOMAIN_ID`，检查 `ros2 node list` 是否有 `/audio_bridge`。后端日志应显示网卡、domain 和 ready；该日志仅表示后端初始化完成，不表示设备音频服务已连通。

`audio backend unavailable` 表示本地 Unix socket 发送失败：检查后端是否退出、socket 路径是否一致和运行用户权限。不要在旧进程仍存活时删除 socket 或启动同路径的新实例；代码会在启动时 unlink 指定路径，第二个实例可能破坏第一个实例通信。运行中更改 socket 参数不会重建连接，需要重启。

## 能看到状态却没有声音

按顺序核对：

1. `/audio/status` 的 `dry_run` 是否为 false、`enabled` 是否为 true。原始 YAML 的门控关闭是预期默认行为。
2. `volume` 是否合适，循环是否启用、`loop_message` 是否为空、有限次数是否已耗尽。
3. 机器人 `vui_service` 是否启用，以及后端日志中的 ServiceSwitch/SetVolume/TtsMaker 返回码。
4. SDK `network_interface` 与 `native_dds_domain_id` 是否正确，物理网络是否能传递所需 DDS 流量。
5. 文本是否过长：限制按 UTF-8 字节计算，中文通常一个汉字占多个字节。

诊断 `OK` 或状态返回码 0 不能单独证明声音输出。直接 SDK TTS probe 可用于定位桥接之外的问题，但会启服务、改音量、发声，须按[操作指南](operator-guide.md)理解其副作用。

## 停止和配置“没有生效”

- `loop off` 只停止后续调度；先 `stop` 再 `disable` 才能在关门前尝试发送 PlayStop。TTS 实际中断能力仍需验证。
- `dry_run`、socket、启动输出开关、最大字节数和持久化路径仅启动时读入。运行时参数 CLI 返回成功不代表内部状态更新。
- 修改 `messages` 不会让多条文字轮播，修改 `loop_message` 才会改变实际循环内容。
- 修改 `interval_sec` 到比保护间隔更小的值，实际周期仍受保护间隔限制；configure 快捷命令直接拒绝小于当前保护间隔的值。
- 参数文件权限不足会导致持久化错误；默认 launch 的自定义参数输入文件与保存目标并不相同。

## 已知实现边界

本项目没有麦克风输入、语音识别、对话服务、本地 TTS 或语速变换。speaker 只区分中文/英文入口；PCM probe 仅生成固定测试音，不是完整音频播放 API。

没有声音完成回调、请求排队、RPC 重试或后端回复超时状态机。循环计数包含未实际播出的尝试；手动请求可以绕过保护间隔。`configure` 并非设备端原子事务，中途失败仅尝试恢复之前的循环开关，不回滚其他参数或设备服务状态。

服务 success 表示本地处理/发送结果，SDK 结果异步到达。状态是手工拼接字符串，特殊字符可能产生非法 JSON。停止调用使用固定应用名，不能宣称能停止所有设备音频或关闭输出后立即静音。

这些限制在首版发布中保留并如实记录，没有通过整理文档修改机器人运行逻辑。后续变更需单独设计和验证。
