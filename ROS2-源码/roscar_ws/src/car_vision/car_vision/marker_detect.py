import cv2
import numpy as np
from geometry_msgs.msg import PoseStamped
from std_msgs.msg import String

from car_vision.vision_io import ImageNode, spin_node


def _detect_aruco(gray):
    if not hasattr(cv2, 'aruco'):
        return None, None
    dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_4X4_50)
    if hasattr(cv2.aruco, 'ArucoDetector'):
        params = cv2.aruco.DetectorParameters()
        corners, ids, _ = cv2.aruco.ArucoDetector(dictionary, params).detectMarkers(gray)
    else:
        params = cv2.aruco.DetectorParameters_create()
        corners, ids, _ = cv2.aruco.detectMarkers(gray, dictionary, parameters=params)
    return corners, ids


class MarkerDetect(ImageNode):
    def __init__(self):
        super().__init__('marker_detect', 'vision/marker/image')
        self.declare_parameter('marker_size', 0.05)
        self.declare_parameter('fx', 500.0)
        self.declare_parameter('fy', 500.0)
        self.result_pub = self.create_publisher(String, 'vision/marker/result', 10)
        self.pose_pub = self.create_publisher(PoseStamped, 'vision/marker/pose', 10)
        self.qr = cv2.QRCodeDetector()
        if not hasattr(cv2, 'aruco'):
            self.get_logger().warn('当前 OpenCV 无 aruco, 只做二维码. 可安装 opencv-contrib-python')

    def _pose(self, corners, header, h, w):
        size = float(self.get_parameter('marker_size').value)
        half = size / 2.0
        obj = np.array([[-half, half, 0], [half, half, 0], [half, -half, 0], [-half, -half, 0]], np.float32)
        fx = float(self.get_parameter('fx').value)
        fy = float(self.get_parameter('fy').value)
        k = np.array([[fx, 0, w / 2.0], [0, fy, h / 2.0], [0, 0, 1]], np.float64)
        ok, rvec, tvec = cv2.solvePnP(obj, corners.reshape(4, 2).astype(np.float32), k, None)
        if not ok:
            return
        rot, _ = cv2.Rodrigues(rvec)
        pose = PoseStamped()
        pose.header = header
        pose.pose.position.x = float(tvec[0])
        pose.pose.position.y = float(tvec[1])
        pose.pose.position.z = float(tvec[2])
        qw = np.sqrt(max(0.0, 1.0 + rot[0, 0] + rot[1, 1] + rot[2, 2])) / 2.0
        pose.pose.orientation.w = float(qw)
        pose.pose.orientation.x = float((rot[2, 1] - rot[1, 2]) / (4.0 * qw)) if qw else 0.0
        pose.pose.orientation.y = float((rot[0, 2] - rot[2, 0]) / (4.0 * qw)) if qw else 0.0
        pose.pose.orientation.z = float((rot[1, 0] - rot[0, 1]) / (4.0 * qw)) if qw else 0.0
        self.pose_pub.publish(pose)

    def on_frame(self, frame, header):
        texts = []
        value, points, _ = self.qr.detectAndDecode(frame)
        if points is not None and value:
            pts = points.reshape(-1, 2).astype(int)
            cv2.polylines(frame, [pts], True, (0, 255, 0), 2)
            cv2.putText(frame, value, tuple(pts[0]), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
            texts.append(f'qr:{value}')
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        corners, ids = _detect_aruco(gray)
        if ids is not None:
            cv2.aruco.drawDetectedMarkers(frame, corners, ids)
            h, w = frame.shape[:2]
            for i, marker_id in enumerate(ids.flatten()):
                texts.append(f'aruco:{int(marker_id)}')
                self._pose(corners[i], header, h, w)
        self.result_pub.publish(String(data=','.join(texts) if texts else 'none'))
        return frame


def main():
    spin_node(MarkerDetect)
