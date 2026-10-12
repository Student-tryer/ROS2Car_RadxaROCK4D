import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource


def generate_launch_description():
    pkg = lambda name: get_package_share_directory(name)

    return LaunchDescription([
        # 启动整车(底盘+描述+雷达+摄像头)
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg('car_launch'), 'launch', 'robot.launch.py'))),

        # 启动 SLAM 建图
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg('car_slam'), 'launch', 'slam.launch.py'))),
    ])