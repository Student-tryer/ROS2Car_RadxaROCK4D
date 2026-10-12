from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('output', default_value='/home/root/maps/room'),
        Node(
            package='nav2_map_server',
            executable='map_saver_cli',
            name='map_saver',
            output='screen',
            arguments=['-f', LaunchConfiguration('output')],
        ),
    ])
