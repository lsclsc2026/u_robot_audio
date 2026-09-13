import os
from pathlib import Path

from ament_index_python.packages import get_package_prefix, get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    package_prefix = Path(get_package_prefix("u_robot_audio_bridge"))
    backend = package_prefix / "lib" / "u_robot_audio_bridge" / "a2_audio_backend"
    # Support both the default isolated install and colcon --merge-install.
    install_root = package_prefix.parent if package_prefix.name == "u_robot_audio_bridge" else package_prefix
    workspace_root = install_root.parent
    sdk_root = Path(os.environ.get("UNITREE_SDK2_ROOT", str(workspace_root.parent / "sdk" / "unitree_sdk2")))
    sdk_arch = "aarch64" if os.uname().machine in {"aarch64", "arm64"} else "x86_64"
    sdk_library_path = str(sdk_root / "thirdparty" / "lib" / sdk_arch)
    native_library_path = sdk_library_path
    if os.environ.get("LD_LIBRARY_PATH"):
        native_library_path += ":" + os.environ["LD_LIBRARY_PATH"]
    default_config = Path(get_package_share_directory("u_robot_audio_bridge")) / "config" / "audio_bridge.yaml"
    config = LaunchConfiguration("parameters_file")
    interface = LaunchConfiguration("network_interface")
    domain = LaunchConfiguration("native_dds_domain_id")
    socket = LaunchConfiguration("backend_socket")
    return LaunchDescription([
        DeclareLaunchArgument("parameters_file", default_value=str(default_config)),
        DeclareLaunchArgument("network_interface", default_value="eth0"),
        DeclareLaunchArgument("native_dds_domain_id", default_value="0"),
        DeclareLaunchArgument("backend_socket", default_value="/tmp/u_robot_a2_audio.sock"),
        ExecuteProcess(cmd=[str(backend), "--network-interface", interface, "--domain-id", domain, "--socket-path", socket],
                       output="screen", respawn=True, respawn_delay=2.0,
                       additional_env={"LD_LIBRARY_PATH": native_library_path}),
        Node(package="u_robot_audio_bridge", executable="audio_bridge_node", name="audio_bridge",
             output="screen", parameters=[config, {
                 "backend_socket": socket,
                 "persistent_config_file": str(default_config),
             }]),
    ])
