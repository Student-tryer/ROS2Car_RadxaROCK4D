import rclpy
from cv_bridge import CvBridge
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import Image


class ImageNode(Node):
    def __init__(self, node_name, debug_topic):
        super().__init__(node_name)
        self.declare_parameter('image_topic', '/image_raw')
        self.bridge = CvBridge()
        topic = self.get_parameter('image_topic').value
        self.create_subscription(Image, topic, self._cb, qos_profile_sensor_data)
        self.debug_pub = self.create_publisher(Image, debug_topic, 10)

    def _cb(self, msg):
        try:
            frame = self.bridge.imgmsg_to_cv2(msg, desired_encoding='bgr8')
        except Exception as exc:
            self.get_logger().error(f'cv_bridge: {exc}')
            return
        out = self.on_frame(frame, msg.header)
        if out is not None:
            out_msg = self.bridge.cv2_to_imgmsg(out, encoding='bgr8')
            out_msg.header = msg.header
            self.debug_pub.publish(out_msg)

    def on_frame(self, frame, header):
        return frame


def clamp(value, low, high):
    return max(low, min(high, value))


def spin_node(node_cls):
    rclpy.init()
    node = node_cls()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()
