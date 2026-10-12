#!/usr/bin/env python3
"""Rock 4D / RK3576 USB 摄像头节点. 发布 /image_raw 与 /camera_info."""

import glob
import threading
import time

import cv2
import rclpy
from cv_bridge import CvBridge
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import CameraInfo, CompressedImage, Image


class OpenCVCamNode(Node):
    def __init__(self):
        super().__init__('opencv_cam')
        self.declare_parameter('video_device', '/dev/video1')
        self.declare_parameter('image_width', 640)
        self.declare_parameter('image_height', 480)
        self.declare_parameter('framerate', 15.0)
        self.declare_parameter('frame_id', 'camera_optical_frame')
        self.declare_parameter('pixel_format', 'mjpeg')
        self.declare_parameter('gst_pipeline', '')
        self.declare_parameter('publish_compressed', True)
        self.declare_parameter('fx', 500.0)
        self.declare_parameter('fy', 500.0)

        self.bridge = CvBridge()
        self.cap = None
        self._stop = False
        self.image_pub = self.create_publisher(Image, 'image_raw', qos_profile_sensor_data)
        self.info_pub = self.create_publisher(CameraInfo, 'camera_info', qos_profile_sensor_data)
        self.comp_pub = self.create_publisher(CompressedImage, 'image_raw/compressed', qos_profile_sensor_data)
        self.thread = threading.Thread(target=self._loop, daemon=True)
        self.thread.start()

    def _open(self):
        gst = self.get_parameter('gst_pipeline').value
        if gst:
            cap = cv2.VideoCapture(gst, cv2.CAP_GSTREAMER)
            return cap
        device = self.get_parameter('video_device').value
        width = int(self.get_parameter('image_width').value)
        height = int(self.get_parameter('image_height').value)
        fps = float(self.get_parameter('framerate').value)
        fmt = self.get_parameter('pixel_format').value
        cap = cv2.VideoCapture(device, cv2.CAP_V4L2)
        fourcc = 'MJPG' if fmt == 'mjpeg' else 'YUYV'
        cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*fourcc))
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        cap.set(cv2.CAP_PROP_FPS, fps)
        cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
        if not cap.isOpened():
            self.get_logger().error(
                f'打不开 {device}, 当前视频设备: {glob.glob("/dev/video*") or "无"}')
            cap.release()
            return None
        self.get_logger().info(f'摄像头已打开 {device} {width}x{height} {fourcc}')
        return cap

    def _camera_info(self, width, height, stamp):
        fx = float(self.get_parameter('fx').value)
        fy = float(self.get_parameter('fy').value)
        info = CameraInfo()
        info.header.stamp = stamp
        info.header.frame_id = self.get_parameter('frame_id').value
        info.width = width
        info.height = height
        info.distortion_model = 'plumb_bob'
        info.d = [0.0, 0.0, 0.0, 0.0, 0.0]
        info.k = [fx, 0.0, width / 2.0, 0.0, fy, height / 2.0, 0.0, 0.0, 1.0]
        info.r = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
        info.p = [fx, 0.0, width / 2.0, 0.0, 0.0, fy, height / 2.0, 0.0, 0.0, 0.0, 1.0, 0.0]
        return info

    def _publish(self, frame):
        stamp = self.get_clock().now().to_msg()
        frame_id = self.get_parameter('frame_id').value
        msg = self.bridge.cv2_to_imgmsg(frame, encoding='bgr8')
        msg.header.stamp = stamp
        msg.header.frame_id = frame_id
        self.image_pub.publish(msg)
        h, w = frame.shape[:2]
        self.info_pub.publish(self._camera_info(w, h, stamp))
        if self.get_parameter('publish_compressed').value:
            ok, buf = cv2.imencode('.jpg', frame, [int(cv2.IMWRITE_JPEG_QUALITY), 80])
            if ok:
                comp = CompressedImage()
                comp.header = msg.header
                comp.format = 'jpeg'
                comp.data = buf.tobytes()
                self.comp_pub.publish(comp)

    def _loop(self):
        while rclpy.ok() and not self._stop:
            if self.cap is None or not self.cap.isOpened():
                self.cap = self._open()
                if self.cap is None:
                    time.sleep(2.0)
                    continue
            ok, frame = self.cap.read()
            if not ok or frame is None:
                self.get_logger().warn('读帧失败, 重开摄像头')
                self.cap.release()
                self.cap = None
                time.sleep(0.5)
                continue
            self._publish(frame)

    def destroy_node(self):
        self._stop = True
        if self.cap is not None:
            self.cap.release()
        super().destroy_node()


def main():
    rclpy.init()
    node = OpenCVCamNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
