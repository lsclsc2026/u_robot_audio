#!/usr/bin/env bash
set -eo pipefail

workspace_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

# ROS environment hooks may reference unset optional variables.
set +u
source /opt/ros/humble/setup.bash
source "${workspace_dir}/install/setup.bash"
set -u

# The native Unitree SDK process uses its own DDS configuration.
unset CYCLONEDDS_URI

exec ros2 launch u_robot_audio_bridge audio_bridge.launch.py "$@"
