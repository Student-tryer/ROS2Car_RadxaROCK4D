import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    slam_params = os.path.join(
        get_package_share_directory('car_slam'), 'config', 'mapper_params_online_async.yaml')
    nav_launch = os.path.join(
        get_package_share_directory('car_nav'), 'launch', 'nav_stack.launch.py')

    return LaunchDescription([
        DeclareLaunchArgument(
            'map_file',
            description='slam_toolbox serialize_map 的文件前缀, 不带扩展名'),
        Node(
            package='slam_toolbox',
            executable='localization_slam_toolbox_node',
            name='slam_toolbox',
            output='screen',
            parameters=[
                slam_params,
                {
                    'mode': 'localization',
                    'map_file_name': ParameterValue(LaunchConfiguration('map_file'), value_type=str),
                },
            ],
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(nav_launch),
        ),
    ])
