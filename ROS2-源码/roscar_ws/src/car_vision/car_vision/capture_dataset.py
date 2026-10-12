import os

import cv2

from car_vision.vision_io import ImageNode, spin_node


class CaptureDataset(ImageNode):
    def __init__(self):
        super().__init__('capture_dataset', 'vision/capture/image')
        self.declare_parameter('save_dir', os.path.expanduser('~/datasets/roscar/images/train'))
        self.declare_parameter('interval_sec', 1.0)
        self.declare_parameter('enable', False)
        self.count = 0
        self.last = None

    def on_frame(self, frame, header):
        if not bool(self.get_parameter('enable').value):
            return frame
        now = self.get_clock().now()
        interval = float(self.get_parameter('interval_sec').value)
        if self.last is not None and (now - self.last).nanoseconds < interval * 1e9:
            return frame
        save_dir = self.get_parameter('save_dir').value
        os.makedirs(save_dir, exist_ok=True)
        path = os.path.join(save_dir, f'{self.count:06d}.jpg')
        cv2.imwrite(path, frame)
        self.count += 1
        self.last = now
        self.get_logger().info(f'保存 {path}')
        cv2.putText(frame, str(self.count), (20, 40), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 255, 0), 2)
        return frame


def main():
    spin_node(CaptureDataset)
