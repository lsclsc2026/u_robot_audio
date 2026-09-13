#!/usr/bin/env bash
set -eo pipefail

workspace_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

source /opt/ros/humble/setup.bash
set -u

cd "${workspace_dir}"
colcon build \
  --symlink-install \
  --base-paths src \
  --event-handlers console_cohesion+
