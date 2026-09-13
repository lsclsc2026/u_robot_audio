#!/usr/bin/env bash
# ROS 2 environment hooks reference optional variables while being sourced,
# so nounset must only be enabled after both setup files have loaded.
set -eo pipefail

workspace_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
volume=60
text="语音播报测试"
speaker_id=0
interface_name=eth0
domain=0

usage() {
  printf '%s\n' \
    "用法: ./scripts/test_tts.sh --volume <0-100> --text <播报内容>" \
    "" \
    "可选参数:" \
    "  --speaker <0|1>       0=中文，1=英文（默认 0）" \
    "  --interface <网卡>    默认 eth0" \
    "  --domain <DDS域>      默认 0" \
    "" \
    "示例:" \
    "  ./scripts/test_tts.sh --volume 100 --text \"前方施工，请注意安全\""
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --volume|-v)
      [[ $# -ge 2 ]] || { echo "--volume 缺少值" >&2; exit 2; }
      volume="$2"
      shift 2
      ;;
    --text|-t)
      [[ $# -ge 2 ]] || { echo "--text 缺少值" >&2; exit 2; }
      text="$2"
      shift 2
      ;;
    --speaker)
      [[ $# -ge 2 ]] || { echo "--speaker 缺少值" >&2; exit 2; }
      speaker_id="$2"
      shift 2
      ;;
    --interface)
      [[ $# -ge 2 ]] || { echo "--interface 缺少值" >&2; exit 2; }
      interface_name="$2"
      shift 2
      ;;
    --domain)
      [[ $# -ge 2 ]] || { echo "--domain 缺少值" >&2; exit 2; }
      domain="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "未知参数: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

[[ "${volume}" =~ ^[0-9]+$ ]] && (( volume >= 0 && volume <= 100 )) || {
  echo "音量必须是 0 到 100 的整数" >&2
  exit 2
}
[[ "${speaker_id}" == "0" || "${speaker_id}" == "1" ]] || {
  echo "speaker 必须是 0（中文）或 1（英文）" >&2
  exit 2
}
[[ -n "${text}" ]] || { echo "播报内容不能为空" >&2; exit 2; }

source /opt/ros/humble/setup.bash
source "${workspace_dir}/install/setup.bash"
set -u

case "$(uname -m)" in
  x86_64) sdk_arch=x86_64 ;;
  aarch64|arm64) sdk_arch=aarch64 ;;
  *) echo "不支持的处理器架构: $(uname -m)" >&2; exit 1 ;;
esac
vendor_lib="${workspace_dir}/../sdk/unitree_sdk2/thirdparty/lib/${sdk_arch}"
export LD_LIBRARY_PATH="${vendor_lib}:${LD_LIBRARY_PATH:-}"
unset CYCLONEDDS_URI

probe="${workspace_dir}/install/u_robot_audio_bridge/lib/u_robot_audio_bridge/a2_tts_probe"
[[ -x "${probe}" ]] || {
  echo "测试程序不存在，请先执行 ./scripts/build.sh" >&2
  exit 1
}

printf '测试参数: volume=%s, speaker=%s, text=%s\n' "${volume}" "${speaker_id}" "${text}"
exec "${probe}" "${interface_name}" "${domain}" "${volume}" "${speaker_id}" "${text}"
