import os

import cv2
import numpy as np
from geometry_msgs.msg import Twist
from std_msgs.msg import Bool, String
from vision_msgs.msg import Detection2D, Detection2DArray, ObjectHypothesisWithPose

from car_vision.vision_io import ImageNode, spin_node

COCO = (
    'person', 'bicycle', 'car', 'motorcycle', 'airplane', 'bus', 'train', 'truck', 'boat',
    'traffic light', 'fire hydrant', 'stop sign', 'parking meter', 'bench', 'bird', 'cat',
    'dog', 'horse', 'sheep', 'cow', 'elephant', 'bear', 'zebra', 'giraffe', 'backpack',
    'umbrella', 'handbag', 'tie', 'suitcase', 'frisbee', 'skis', 'snowboard', 'sports ball',
    'kite', 'baseball bat', 'baseball glove', 'skateboard', 'surfboard', 'tennis racket',
    'bottle', 'wine glass', 'cup', 'fork', 'knife', 'spoon', 'bowl', 'banana', 'apple',
    'sandwich', 'orange', 'broccoli', 'carrot', 'hot dog', 'pizza', 'donut', 'cake',
    'chair', 'couch', 'potted plant', 'bed', 'dining table', 'toilet', 'tv', 'laptop',
    'mouse', 'remote', 'keyboard', 'cell phone', 'microwave', 'oven', 'toaster', 'sink',
    'refrigerator', 'book', 'clock', 'vase', 'scissors', 'teddy bear', 'hair drier', 'toothbrush',
)


def letterbox(image, size):
    h, w = image.shape[:2]
    scale = min(size / h, size / w)
    nw, nh = int(round(w * scale)), int(round(h * scale))
    resized = cv2.resize(image, (nw, nh))
    canvas = np.full((size, size, 3), 114, np.uint8)
    left = (size - nw) // 2
    top = (size - nh) // 2
    canvas[top:top + nh, left:left + nw] = resized
    return canvas, scale, left, top


def parse_yolo(output, conf_th, iou_th, scale, left, top, names):
    pred = np.squeeze(np.array(output))
    if pred.ndim != 2:
        return []
    if pred.shape[0] < pred.shape[1]:
        pred = pred.T
    if pred.shape[1] < 6:
        return []
    boxes = pred[:, :4]
    cls_scores = pred[:, 4:]
    class_ids = np.argmax(cls_scores, axis=1)
    conf = cls_scores[np.arange(len(class_ids)), class_ids]
    keep = conf >= conf_th
    boxes, conf, class_ids = boxes[keep], conf[keep], class_ids[keep]
    if len(boxes) == 0:
        return []
    xyxy = np.empty_like(boxes)
    xyxy[:, 0] = boxes[:, 0] - boxes[:, 2] / 2.0
    xyxy[:, 1] = boxes[:, 1] - boxes[:, 3] / 2.0
    xyxy[:, 2] = boxes[:, 0] + boxes[:, 2] / 2.0
    xyxy[:, 3] = boxes[:, 1] + boxes[:, 3] / 2.0
    xyxy[:, [0, 2]] = (xyxy[:, [0, 2]] - left) / scale
    xyxy[:, [1, 3]] = (xyxy[:, [1, 3]] - top) / scale
    rects = np.stack([xyxy[:, 0], xyxy[:, 1], xyxy[:, 2] - xyxy[:, 0], xyxy[:, 3] - xyxy[:, 1]], axis=1)
    idxs = cv2.dnn.NMSBoxes(rects.tolist(), conf.tolist(), conf_th, iou_th)
    if idxs is None or len(idxs) == 0:
        return []
    found = []
    for i in np.array(idxs).reshape(-1):
        cid = int(class_ids[i])
        name = names[cid] if 0 <= cid < len(names) else f'class_{cid}'
        x1, y1, x2, y2 = [float(v) for v in xyxy[i]]
        found.append((name, float(conf[i]), x1, y1, x2, y2))
    return found


class YoloDetect(ImageNode):
    def __init__(self):
        super().__init__('yolo_detect', 'vision/yolo/image')
        self.declare_parameter('backend', 'auto')
        self.declare_parameter('model_path', '')
        self.declare_parameter('rknn_path', '')
        self.declare_parameter('imgsz', 320)
        self.declare_parameter('conf', 0.45)
        self.declare_parameter('iou', 0.45)
        self.declare_parameter('use_coco_names', True)
        self.declare_parameter('class_names', ['person', 'bottle', 'ball', 'stop_sign', 'traffic_light', 'obstacle'])
        self.declare_parameter('stop_classes', ['stop sign', 'stop_sign'])
        self.declare_parameter('react_enable', False)
        self.det_pub = self.create_publisher(Detection2DArray, 'vision/yolo/detections', 10)
        self.result_pub = self.create_publisher(String, 'vision/yolo/result', 10)
        self.estop_pub = self.create_publisher(Bool, 'vision/estop', 10)
        self.cmd_pub = self.create_publisher(Twist, 'vision/yolo/cmd_vel', 10)
        self.net = None
        self.rknn = None
        self.backend = None
        self._logged_miss = False
        self._load()

    def _find(self, given, extra):
        cands = [given] if given else []
        cands.extend(extra)
        for path in cands:
            if path and os.path.isfile(os.path.expanduser(path)):
                return os.path.expanduser(path)
        return ''

    def _load(self):
        backend = self.get_parameter('backend').value
        rknn_path = self._find(self.get_parameter('rknn_path').value, ['~/models/yolov8n.rknn'])
        onnx_path = self._find(self.get_parameter('model_path').value, ['~/models/yolov8n.onnx'])
        if backend in ('auto', 'rknn') and rknn_path:
            try:
                from rknnlite.api import RKNNLite
                rknn = RKNNLite()
                if rknn.load_rknn(rknn_path) != 0 or rknn.init_runtime() != 0:
                    raise RuntimeError('init failed')
                self.rknn = rknn
                self.backend = 'rknn'
                self.get_logger().info(f'RK3576 NPU: {rknn_path}')
                return
            except Exception as exc:
                self.get_logger().warn(f'RKNN 不可用: {exc}')
                if backend == 'rknn':
                    return
        if onnx_path:
            self.net = cv2.dnn.readNetFromONNX(onnx_path)
            self.net.setPreferableBackend(cv2.dnn.DNN_BACKEND_OPENCV)
            self.net.setPreferableTarget(cv2.dnn.DNN_TARGET_CPU)
            self.backend = 'opencv'
            self.get_logger().info(f'ONNX CPU: {onnx_path}')
            return
        self.get_logger().error('未找到模型. 把 yolov8n.onnx 放到 ~/models/ , 或设置 model_path')

    def _names(self):
        if bool(self.get_parameter('use_coco_names').value):
            return COCO
        return list(self.get_parameter('class_names').value)

    def _infer(self, canvas):
        if self.backend == 'opencv':
            blob = cv2.dnn.blobFromImage(canvas, 1.0 / 255.0, (canvas.shape[1], canvas.shape[0]), swapRB=True, crop=False)
            self.net.setInput(blob)
            out = self.net.forward(self.net.getUnconnectedOutLayersNames())
            return out[0] if isinstance(out, (list, tuple)) else out
        rgb = cv2.cvtColor(canvas, cv2.COLOR_BGR2RGB)
        for inp in (np.expand_dims(rgb, 0), rgb):
            try:
                outs = self.rknn.inference(inputs=[inp])
                return outs[0] if outs else None
            except Exception as exc:
                self.get_logger().warn(f'rknn 推理失败 {getattr(inp, "shape", "")}: {exc}')
        return None

    def on_frame(self, frame, header):
        if self.backend is None:
            if not self._logged_miss:
                self._logged_miss = True
            return frame
        size = int(self.get_parameter('imgsz').value)
        canvas, scale, left, top = letterbox(frame, size)
        try:
            output = self._infer(canvas)
        except Exception as exc:
            self.get_logger().error(f'推理失败, 检查 imgsz 是否和导出模型一致: {exc}')
            return frame
        if output is None:
            return frame
        found = parse_yolo(
            output,
            float(self.get_parameter('conf').value),
            float(self.get_parameter('iou').value),
            scale, left, top, self._names())
        arr = Detection2DArray()
        arr.header = header
        labels = []
        stop_names = set(self.get_parameter('stop_classes').value)
        stopped = False
        for name, score, x1, y1, x2, y2 in found:
            det = Detection2D()
            det.header = header
            det.bbox.center.x = (x1 + x2) / 2.0
            det.bbox.center.y = (y1 + y2) / 2.0
            det.bbox.size_x = x2 - x1
            det.bbox.size_y = y2 - y1
            hyp = ObjectHypothesisWithPose()
            hyp.hypothesis.class_id = name
            hyp.hypothesis.score = score
            det.results.append(hyp)
            arr.detections.append(det)
            labels.append(f'{name}:{score:.2f}')
            cv2.rectangle(frame, (int(x1), int(y1)), (int(x2), int(y2)), (0, 255, 0), 2)
            cv2.putText(frame, labels[-1], (int(x1), max(0, int(y1) - 6)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
            if name in stop_names and score >= float(self.get_parameter('conf').value):
                stopped = True
        self.det_pub.publish(arr)
        self.result_pub.publish(String(data=','.join(labels) if labels else 'none'))
        self.estop_pub.publish(Bool(data=stopped))
        if bool(self.get_parameter('react_enable').value) and stopped:
            self.cmd_pub.publish(Twist())
        return frame


def main():
    spin_node(YoloDetect)
