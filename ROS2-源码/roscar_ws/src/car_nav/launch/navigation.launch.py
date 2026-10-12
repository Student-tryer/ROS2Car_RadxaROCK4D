import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    params = os.path.join(get_package_share_directory('car_nav'), 'config', 'nav2_params.yaml')
    bringup = os.path.join(get_package_share_directory('nav2_bringup'), 'launch', 'bringup_launch.py')

    return LaunchDescription([
        DeclareLaunchArgument('map', description='map_saver 生成的 yaml 绝对路径'),
        DeclareLaunchArgument('use_sim_time', default_value='false'),
        DeclareLaunchArgument('autostart', default_value='true'),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(bringup),
            launch_arguments={
                'map': LaunchConfiguration('map'),
                'use_sim_time': LaunchConfiguration('use_sim_time'),
                'params_file': params,
                'autostart': LaunchConfiguration('autostart'),
                'slam': 'False',
            }.items(),
        ),
    ])
