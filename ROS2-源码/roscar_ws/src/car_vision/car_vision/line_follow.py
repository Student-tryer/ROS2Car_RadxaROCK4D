import cv2
from geometry_msgs.msg import Twist
from std_msgs.msg import String

from car_vision.vision_io import ImageNode, clamp, spin_node


class LineFollow(ImageNode):
    def __init__(self):
        super().__init__('line_follow', 'vision/line/image')
        self.declare_parameter('line_color', 'black')
        self.declare_parameter('thresh', 60)
        self.declare_parameter('roi_top', 0.55)
        self.declare_parameter('min_area', 400)
        self.declare_parameter('follow_enable', False)
        self.declare_parameter('linear_speed', 0.10)
        self.declare_parameter('max_angular', 0.8)
        self.declare_parameter('kp_angular', 1.1)
        self.declare_parameter('search_angular', 0.25)
        self.result_pub = self.create_publisher(String, 'vision/line/result', 10)
        self.cmd_pub = self.create_publisher(Twist, 'vision/line/cmd_vel', 10)
        self._seen = False

    def on_frame(self, frame, header):
        h, w = frame.shape[:2]
        y0 = int(h * float(self.get_parameter('roi_top').value))
        roi = frame[y0:, :]
        gray = cv2.cvtColor(roi, cv2.COLOR_BGR2GRAY)
        gray = cv2.GaussianBlur(gray, (5, 5), 0)
        thresh = int(self.get_parameter('thresh').value)
        mode = cv2.THRESH_BINARY_INV if self.get_parameter('line_color').value == 'black' else cv2.THRESH_BINARY
        _, mask = cv2.threshold(gray, thresh, 255, mode)
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        min_area = int(self.get_parameter('min_area').value)
        follow = bool(self.get_parameter('follow_enable').value)
        best = None
        for cnt in contours:
            area = cv2.contourArea(cnt)
            if area >= min_area and (best is None or area > best[0]):
                best = (area, cnt)
        cv2.line(frame, (0, y0), (w, y0), (255, 255, 0), 1)
        if best is None:
            self.result_pub.publish(String(data='line none'))
            if follow:
                twist = Twist()
                twist.angular.z = float(self.get_parameter('search_angular').value) if self._seen else 0.0
                self.cmd_pub.publish(twist)
            return frame
        area, cnt = best
        m = cv2.moments(cnt)
        cx = int(m['m10'] / m['m00']) if m['m00'] else w // 2
        cy = int(m['m01'] / m['m00']) + y0 if m['m00'] else (y0 + h) // 2
        cv2.circle(frame, (cx, cy), 8, (0, 0, 255), -1)
        err = (cx - w / 2.0) / (w / 2.0)
        self.result_pub.publish(String(data=f'line err={err:.2f}'))
        if follow:
            twist = Twist()
            twist.angular.z = 0.0 if abs(err) < 0.06 else clamp(
                -float(self.get_parameter('kp_angular').value) * err,
                -float(self.get_parameter('max_angular').value),
                float(self.get_parameter('max_angular').value))
            twist.linear.x = float(self.get_parameter('linear_speed').value) * (1.0 - min(1.0, abs(err)))
            self.cmd_pub.publish(twist)
            self._seen = True
        return frame


def main():
    spin_node(LineFollow)
