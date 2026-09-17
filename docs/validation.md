# 验证记录

检查日期：2026-09-13。来源为开发容器中的独立音频工作区快照；发布整理保留默认 YAML、SDK 调用及调度行为，仅修正 launch 中绑定个人目录的 SDK 运行时路径。

## 已执行检查

| 检查 | 结果与边界 |
|---|---|
| 4 个 Shell 脚本 `bash -n` | 通过；没有执行脚本实际控制逻辑 |
| launch Python AST | 通过；仅语法解析 |
| package.xml 与 YAML 解析 | 通过；确认 package 原许可证声明保留，真实默认值与文档一致 |
| launch 路径推导 | 3 个模拟环境案例通过：独立 ARM64 安装、合并 x86_64 安装、环境变量覆盖；不启动 ROS 或 SDK |
| Markdown 相对文件链接、尾随空白、文件体积 | 静态扫描通过，参见下方复查命令 |
| SDK 身份核对 | 对来源容器只读检查 HEAD、工作区状态与两架构静态库 SHA256，记录于第三方说明 |

另已在独立、无外部网络的容器中使用发布源码从空 build/install 完整编译：`colcon build --executor sequential --symlink-install` 成功，五个可执行程序均生成。环境为 x86_64、GCC 11.4、ROS 2 Humble 与固定版本 SDK，来源是当前开发容器文件系统的本地副本。未挂载机器人设备或调用原生接口。

`colcon test` 和 `colcon test-result --verbose` 返回 0，但结果为 **0 tests**：本包尚未注册 C++ 自动测试，不能把命令成功表述为功能测试覆盖。编译中有第三方 SDK 头文件警告，不影响本次构建完成。

本记录未执行任何会发声、调整音量、切换机器人服务或运动的程序。编译成功不构成 RPC 连通性或音频效果验收；新 Dockerfile 从零重建的环境还需单独核验。

## 可复查的静态命令

在仓库根目录执行：

```bash
for script in scripts/*.sh; do bash -n "$script" || exit; done
python3 - <<'PY'
import ast
import re
import subprocess
from pathlib import Path
import xml.etree.ElementTree as ET
import yaml
root = Path('.')
for path in root.glob('src/**/launch/*.py'):
    ast.parse(path.read_text(), filename=str(path))
ET.parse(root / 'src/u_robot_audio_bridge/package.xml')
params = yaml.safe_load((root / 'src/u_robot_audio_bridge/config/audio_bridge.yaml').read_text())['audio_bridge']['ros__parameters']
assert params['dry_run'] is False
assert params['output_enabled_on_start'] is False
assert params['loop_enabled'] is True and params['volume'] == 100
links = 0
files = [root / name for name in subprocess.check_output(['git', 'ls-files', '-z']).decode().split('\0') if name]
for path in files:
    assert path.stat().st_size < 10 * 1024 * 1024, path
    text = path.read_text()
    assert all(line == line.rstrip() for line in text.splitlines()), path
    if path.suffix == '.md':
        for target in re.findall(r'\[[^\]]*\]\(([^)]+)\)', text):
            if '://' in target or target.startswith('#'):
                continue
            assert (path.parent / target.split('#')[0]).exists(), (path, target)
            links += 1
print(f'PASS: syntax, XML, YAML defaults, {links} local links, {len(files)} text files below 10 MiB and no trailing whitespace')
PY
```

Python 配置检查需要 PyYAML。链接检查验证本地目标文件存在，不检查外部私有仓库权限或网络可达性。实际 shell/Python 检查输出记录在首版发布整理报告中。

## 待补充的验证

- 在新 Dockerfile 构建的环境中复核编译；与本次基于现有环境副本的编译分别记录。
- 仅运行 ROS 节点的 dry-run：检查参数、话题、服务和无后端时的处理。
- 实机单次中文/英文 TTS、音量设置与服务开关返回值，并实际试听。
- 循环间隔、有限次数、修改文字、暂停/恢复，核对调度次数与实际播放的区别。
- 持续播放期间的 stop → disable 顺序，以及 PlayStop 对当前固件原生 TTS 的中断行为。
- 后端退出/重启、无 DDS 连通、参数文件不可写和特殊字符状态处理。

后续每项实机记录应包含设备/固件、SDK 校验值、源码提交、命令、预期与实际结果，不用源码存在或返回码成功替代实际声音证明。
