import cv2
import numpy as np
from geometry_msgs.msg import Twist
from std_msgs.msg import String

from car_vision.vision_io import ImageNode, clamp, spin_node

RANGES = {
    'red': [((0, 100, 60), (10, 255, 255)), ((160, 100, 60), (180, 255, 255))],
    'green': [((35, 80, 50), (85, 255, 255))],
    'blue': [((90, 80, 50), (130, 255, 255))],
    'yellow': [((18, 80, 80), (38, 255, 255))],
}


class ColorDetect(ImageNode):
    def __init__(self):
        super().__init__('color_detect', 'vision/color/image')
        self.declare_parameter('target_color', 'red')
        self.declare_parameter('min_area', 800)
        self.declare_parameter('follow_enable', False)
        self.declare_parameter('linear_speed', 0.12)
        self.declare_parameter('max_angular', 0.7)
        self.declare_parameter('kp_angular', 0.8)
        self.result_pub = self.create_publisher(String, 'vision/color/result', 10)
        self.cmd_pub = self.create_publisher(Twist, 'vision/color/cmd_vel', 10)
        self._seen = False

    def _mask(self, hsv, color):
        ranges = RANGES.get(color, RANGES['red'])
        mask = None
        for low, high in ranges:
            part = cv2.inRange(hsv, np.array(low), np.array(high))
            mask = part if mask is None else cv2.bitwise_or(mask, part)
        kernel = np.ones((5, 5), np.uint8)
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)
        return cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)

    def on_frame(self, frame, header):
        color = self.get_parameter('target_color').value
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        mask = self._mask(hsv, color)
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        min_area = int(self.get_parameter('min_area').value)
        follow = bool(self.get_parameter('follow_enable').value)
        h, w = frame.shape[:2]
        best = None
        for cnt in contours:
            area = cv2.contourArea(cnt)
            if area < min_area:
                continue
            if best is None or area > best[0]:
                best = (area, cnt)
        if best is None:
            if follow and self._seen:
                self.cmd_pub.publish(Twist())
                self._seen = False
            self.result_pub.publish(String(data=f'{color} none'))
            return frame
        area, cnt = best
        m = cv2.moments(cnt)
        cx = int(m['m10'] / m['m00']) if m['m00'] else w // 2
        cy = int(m['m01'] / m['m00']) if m['m00'] else h // 2
        cv2.drawContours(frame, [cnt], -1, (0, 255, 0), 2)
        cv2.circle(frame, (cx, cy), 6, (0, 0, 255), -1)
        err = (cx - w / 2.0) / (w / 2.0)
        self.result_pub.publish(String(data=f'{color} area={int(area)} err={err:.2f}'))
        if follow:
            twist = Twist()
            twist.angular.z = 0.0 if abs(err) < 0.08 else clamp(
                -float(self.get_parameter('kp_angular').value) * err,
                -float(self.get_parameter('max_angular').value),
                float(self.get_parameter('max_angular').value))
            twist.linear.x = 0.0 if area > 40000 else float(self.get_parameter('linear_speed').value) * (1.0 - min(1.0, abs(err)))
            self.cmd_pub.publish(twist)
            self._seen = True
        return frame


def main():
    spin_node(ColorDetect)
