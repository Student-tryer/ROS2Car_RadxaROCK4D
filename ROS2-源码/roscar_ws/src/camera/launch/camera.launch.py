import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import Node


def generate_launch_description():
    pkg_share = get_package_share_directory('camera')
    backend = LaunchConfiguration('backend')
    opencv_params = os.path.join(pkg_share, 'config', 'opencv_cam.yaml')
    usb_params = os.path.join(pkg_share, 'config', 'usb_cam.yaml')

    return LaunchDescription([
        DeclareLaunchArgument('backend', default_value='opencv',
                              description='opencv 或 usb_cam'),
        Node(
            package='camera',
            executable='opencv_cam_node.py',
            name='opencv_cam',
            output='screen',
            parameters=[opencv_params],
            condition=IfCondition(PythonExpression(["'", backend, "' == 'opencv'"])),
        ),
        Node(
            package='usb_cam',
            executable='usb_cam_node_exe',
            name='usb_cam_node',
            output='screen',
            parameters=[usb_params],
            condition=IfCondition(PythonExpression(["'", backend, "' == 'usb_cam'"])),
        ),
    ])
