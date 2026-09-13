#!/usr/bin/env bash
set -eo pipefail

workspace_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
source /opt/ros/humble/setup.bash
source "${workspace_dir}/install/setup.bash"
set -u

node="/audio_bridge"

usage() {
  printf '%s\n' \
    "用法: ./scripts/audio_ctl.sh <命令> [参数]" \
    "" \
    "  configure --volume N --text 文字 --interval 秒" \
    "                          一次设置音量、循环内容和间隔，并启动循环" \
    "  say <文字>              立即播报一条文字" \
    "  volume <0-100>          修改音量" \
    "  interval <秒>           修改循环播报间隔（>= 0.5）" \
    "  message <文字...>       设置一条循环播报内容" \
    "  loop on|off             启动或暂停循环播报" \
    "  enable                  显式打开真实音频输出" \
    "  voice-service on|off    启用或关闭机器人 vui_service" \
    "  disable                 关闭真实音频输出" \
    "  stop                    停止播报并关闭循环" \
    "  status                  查看最近状态" \
    "  params                  查看全部可调参数"
}

require_audio_node() {
  if ! ros2 node list 2>/dev/null | grep -Fxq "${node}"; then
    printf '%s\n' \
      "未发现 ${node}，导航不会受到影响，但语音模块需要单独启动。" \
      "请在另一个 Docker 终端运行：" \
      "  cd ${workspace_dir}" \
      "  ros2 launch u_robot_audio_bridge audio_bridge.launch.py" >&2
    exit 1
  fi
}

require_value() {
  if [[ $# -eq 0 ]]; then
    usage
    exit 2
  fi
}

command="${1:-help}"
shift || true

case "${command}" in
  configure)
    volume_value=""
    text_value=""
    interval_value=""
    while [[ $# -gt 0 ]]; do
      case "$1" in
        --volume|-v)
          [[ $# -ge 2 ]] || { echo "--volume 缺少值" >&2; exit 2; }
          volume_value="$2"; shift 2 ;;
        --text|-t)
          [[ $# -ge 2 ]] || { echo "--text 缺少值" >&2; exit 2; }
          text_value="$2"; shift 2 ;;
        --interval|-i)
          [[ $# -ge 2 ]] || { echo "--interval 缺少值" >&2; exit 2; }
          interval_value="$2"; shift 2 ;;
        *) echo "未知参数: $1" >&2; usage >&2; exit 2 ;;
      esac
    done
    [[ "${volume_value}" =~ ^[0-9]+$ ]] &&
      (( volume_value >= 0 && volume_value <= 100 )) || {
        echo "音量必须是 0 到 100 的整数" >&2; exit 2;
      }
    [[ -n "${text_value}" ]] || { echo "播报内容不能为空" >&2; exit 2; }
    [[ "${interval_value}" =~ ^[0-9]+([.][0-9]+)?$ ]] &&
      awk -v value="${interval_value}" 'BEGIN { exit !(value >= 0.5) }' || {
        echo "循环间隔必须是不小于 0.5 的秒数" >&2; exit 2;
      }
    if [[ "${interval_value}" != *.* ]]; then
      interval_value="${interval_value}.0"
    fi

    require_audio_node
    guard_value="$(ros2 param get "${node}" minimum_speech_guard_sec | awk '{print $NF}')"
    awk -v interval="${interval_value}" -v guard="${guard_value}" \
      'BEGIN { exit !(interval >= guard) }' || {
        printf '循环间隔不能小于当前安全间隔 %s 秒\n' "${guard_value}" >&2
        exit 2
      }
    previous_loop="$(ros2 param get "${node}" loop_enabled | awk '{print $NF}')"
    restore_loop() {
      ros2 param set "${node}" loop_enabled "${previous_loop}" >/dev/null 2>&1 || true
    }
    trap restore_loop ERR
    ros2 param set "${node}" loop_enabled false
    ros2 param set "${node}" volume "${volume_value}"
    ros2 param set "${node}" loop_message "${text_value}"
    ros2 param set "${node}" interval_sec "${interval_value}"
    ros2 service call /audio_bridge/enable_voice_service std_srvs/srv/SetBool \
      "{data: true}" >/dev/null
    ros2 service call /audio_bridge/enable_output std_srvs/srv/SetBool \
      "{data: true}" >/dev/null
    ros2 param set "${node}" loop_enabled true
    trap - ERR
    printf '循环播报已启动: volume=%s, interval=%s 秒, text=%s\n' \
      "${volume_value}" "${interval_value}" "${text_value}"
    ;;
  say)
    require_audio_node
    require_value "$@"
    text="$*"
    if [[ "${text}" == *"'"* ]]; then
      echo "当前快捷命令不接受英文单引号，请改用 Foxglove Publish 面板。" >&2
      exit 2
    fi
    ros2 topic pub --once /operator/audio/speak std_msgs/msg/String "{data: '${text}'}"
    ;;
  volume)
    require_audio_node
    require_value "$@"
    ros2 param set "${node}" volume "$1"
    ;;
  interval)
    require_audio_node
    require_value "$@"
    interval_value="$1"
    if [[ "${interval_value}" != *.* && "${interval_value}" != *e* && "${interval_value}" != *E* ]]; then
      interval_value="${interval_value}.0"
    fi
    ros2 param set "${node}" interval_sec "${interval_value}"
    ;;
  message|messages)
    require_audio_node
    require_value "$@"
    text="$*"
    if [[ "${text}" == *"'"* ]]; then
      echo "循环文字暂不接受英文单引号。" >&2
      exit 2
    fi
    ros2 param set "${node}" loop_message "${text}"
    ;;
  loop)
    require_audio_node
    require_value "$@"
    case "$1" in
      on) ros2 param set "${node}" loop_enabled true ;;
      off) ros2 param set "${node}" loop_enabled false ;;
      *) echo "loop 只接受 on 或 off" >&2; exit 2 ;;
    esac
    ;;
  enable)
    require_audio_node
    ros2 service call /audio_bridge/enable_output std_srvs/srv/SetBool "{data: true}"
    ;;
  voice-service)
    require_audio_node
    require_value "$@"
    case "$1" in
      on) ros2 service call /audio_bridge/enable_voice_service std_srvs/srv/SetBool "{data: true}" ;;
      off) ros2 service call /audio_bridge/enable_voice_service std_srvs/srv/SetBool "{data: false}" ;;
      *) echo "voice-service 只接受 on 或 off" >&2; exit 2 ;;
    esac
    ;;
  disable)
    require_audio_node
    ros2 service call /audio_bridge/enable_output std_srvs/srv/SetBool "{data: false}"
    ;;
  stop)
    require_audio_node
    ros2 topic pub --once /operator/audio/stop std_msgs/msg/Empty "{}"
    ;;
  status)
    require_audio_node
    ros2 topic echo --once /audio/status
    ;;
  params)
    require_audio_node
    ros2 param dump "${node}"
    ;;
  help|-h|--help)
    usage
    ;;
  *)
    echo "未知命令: ${command}" >&2
    usage
    exit 2
    ;;
esac
