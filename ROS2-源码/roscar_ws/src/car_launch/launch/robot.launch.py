import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (DeclareLaunchArgument, IncludeLaunchDescription)
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    # 启动选项: 是否启动摄像头 / 是否启动SLAM
    with_camera = LaunchConfiguration('with_camera')
    with_slam = LaunchConfiguration('with_slam')

    pkg = lambda name: get_package_share_directory(name)

    return LaunchDescription([
        DeclareLaunchArgument('with_camera', default_value='true'),
        DeclareLaunchArgument('with_slam', default_value='false'),

        # 底盘驱动 (car_base)
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg('car_base'), 'launch', 'car_base.launch.py'))),

        # 机器人描述 + TF (car_description)
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg('car_description'), 'launch', 'description.launch.py'))),

        # 激光雷达 (ydlidar_ros2_driver 官方驱动)
        # 通过 LIDAR_TYPE 环境变量选择型号: x3 / 4ros
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg('ydlidar_ros2_driver'), 'launch', 'ydlidar_launch.py'))),

        # 摄像头 (可选)
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg('camera'), 'launch', 'camera.launch.py')),
            condition=IfCondition(with_camera)),

        # SLAM 建图 (可选)
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg('car_slam'), 'launch', 'slam.launch.py')),
            condition=IfCondition(with_slam)),
    ])