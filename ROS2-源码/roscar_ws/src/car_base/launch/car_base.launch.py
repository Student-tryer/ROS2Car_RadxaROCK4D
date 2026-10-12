from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        Node(
            package='car_base',
            executable='rock4d_driver',
            name='rock4d_driver',
            output='screen',
            parameters=[{
                'port': '/dev/ttyUSB0',
                'baud': 115200,
                'request_hz': 50,
                'wheel_base': 0.16872,
                'wheel_circumference': 0.21834,
                'encoder_ppr': 336,
                'loop_hz': 200,
                'test_speed': 0.3,
            }],
        )
    ])