import os
from glob import glob

from setuptools import setup

package_name = 'car_vision'

setup(
    name=package_name,
    version='0.1.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join('share', package_name, 'launch'), glob('launch/*.py')),
        (os.path.join('share', package_name, 'config'), glob('config/*')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='root',
    maintainer_email='root@roscar.local',
    description='Color, line, marker and YOLO vision for the ROS car',
    license='Apache-2.0',
    entry_points={
        'console_scripts': [
            'color_detect = car_vision.color_detect:main',
            'line_follow = car_vision.line_follow:main',
            'marker_detect = car_vision.marker_detect:main',
            'yolo_detect = car_vision.yolo_detect:main',
            'capture_dataset = car_vision.capture_dataset:main',
            'train_yolo = car_vision.train_yolo:main',
            'export_rknn = car_vision.export_rknn:main',
            'prepare_dataset = car_vision.prepare_dataset:main',
        ],
    },
)
