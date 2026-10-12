import argparse
import os


NAMES = ['person', 'bottle', 'ball', 'stop_sign', 'traffic_light', 'obstacle']


def main():
    parser = argparse.ArgumentParser(description='建立 YOLO 数据集目录和 yaml')
    parser.add_argument('--root', default=os.path.expanduser('~/datasets/roscar'))
    args = parser.parse_args()
    root = os.path.abspath(args.root)
    for split in ('train', 'val'):
        os.makedirs(os.path.join(root, 'images', split), exist_ok=True)
        os.makedirs(os.path.join(root, 'labels', split), exist_ok=True)
    yaml_path = os.path.join(root, 'roscar.yaml')
    lines = [
        f'path: {root}',
        'train: images/train',
        'val: images/val',
        'names:',
    ]
    lines.extend(f'  {i}: {name}' for i, name in enumerate(NAMES))
    with open(yaml_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    print(yaml_path)
    print('每类至少 50 张. 标签是 YOLO txt: class cx cy w h, 都是 0~1')
    print('车上采图: ros2 run car_vision capture_dataset --ros-args -p enable:=true')


if __name__ == '__main__':
    main()
