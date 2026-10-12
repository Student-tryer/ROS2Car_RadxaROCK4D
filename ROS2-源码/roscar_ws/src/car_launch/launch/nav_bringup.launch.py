import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    pkg = lambda name: get_package_share_directory(name)
    return LaunchDescription([
        DeclareLaunchArgument('map', description='map yaml 绝对路径'),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(pkg('car_launch'), 'launch', 'robot.launch.py')),
            launch_arguments={'with_camera': 'false', 'with_slam': 'false'}.items(),
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(pkg('car_nav'), 'launch', 'navigation.launch.py')),
            launch_arguments={'map': LaunchConfiguration('map')}.items(),
        ),
    ])
