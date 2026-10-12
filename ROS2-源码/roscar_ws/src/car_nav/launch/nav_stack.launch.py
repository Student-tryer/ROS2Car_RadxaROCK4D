import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    """只起 Nav2 规划控制, 不启 AMCL. 用于已有 map->odom 的定位."""
    params = os.path.join(get_package_share_directory('car_nav'), 'config', 'nav2_params.yaml')
    nav = os.path.join(get_package_share_directory('nav2_bringup'), 'launch', 'navigation_launch.py')

    return LaunchDescription([
        DeclareLaunchArgument('use_sim_time', default_value='false'),
        DeclareLaunchArgument('autostart', default_value='true'),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(nav),
            launch_arguments={
                'use_sim_time': LaunchConfiguration('use_sim_time'),
                'params_file': params,
                'autostart': LaunchConfiguration('autostart'),
            }.items(),
        ),
    ])
