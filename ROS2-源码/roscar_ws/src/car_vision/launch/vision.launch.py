import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import Node


def _mode(*names):
    listing = ', '.join(f"'{name}'" for name in names)
    return IfCondition(PythonExpression(["'", LaunchConfiguration('mode'), f"' in ({listing})"]))


def generate_launch_description():
    share = get_package_share_directory('car_vision')
    cfg = lambda name: os.path.join(share, 'config', name)

    return LaunchDescription([
        DeclareLaunchArgument('mode', default_value='color',
                              description='color | color_follow | line | marker | yolo | all'),
        Node(
            package='car_vision', executable='color_detect', name='color_detect', output='screen',
            parameters=[cfg('color.yaml')],
            condition=_mode('color', 'all'),
        ),
        Node(
            package='car_vision', executable='color_detect', name='color_detect', output='screen',
            parameters=[cfg('color_follow.yaml')],
            remappings=[('vision/color/cmd_vel', 'cmd_vel')],
            condition=_mode('color_follow'),
        ),
        Node(
            package='car_vision', executable='line_follow', name='line_follow', output='screen',
            parameters=[cfg('line.yaml')],
            condition=_mode('all'),
        ),
        Node(
            package='car_vision', executable='line_follow', name='line_follow', output='screen',
            parameters=[cfg('line_drive.yaml')],
            remappings=[('vision/line/cmd_vel', 'cmd_vel')],
            condition=_mode('line'),
        ),
        Node(
            package='car_vision', executable='marker_detect', name='marker_detect', output='screen',
            parameters=[cfg('marker.yaml')],
            condition=_mode('marker', 'all'),
        ),
        Node(
            package='car_vision', executable='yolo_detect', name='yolo_detect', output='screen',
            parameters=[cfg('yolo.yaml')],
            condition=_mode('yolo', 'all'),
        ),
    ])
