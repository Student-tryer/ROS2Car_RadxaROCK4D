import argparse
import os


def main():
    parser = argparse.ArgumentParser(description='训练 YOLOv8n, 并导出 RK3576 可用的 ONNX')
    parser.add_argument('--data', default=os.path.expanduser('~/datasets/roscar/roscar.yaml'))
    parser.add_argument('--model', default='yolov8n.pt')
    parser.add_argument('--epochs', type=int, default=80)
    parser.add_argument('--imgsz', type=int, default=320)
    parser.add_argument('--batch', type=int, default=8)
    parser.add_argument('--device', default='')
    parser.add_argument('--project', default=os.path.expanduser('~/runs/roscar'))
    parser.add_argument('--name', default='yolov8n')
    parser.add_argument('--pretrained', action='store_true',
                        help='不训练, 只把官方 yolov8n 导出成 ONNX, 用于先跑通 person/bottle')
    args = parser.parse_args()
    try:
        from ultralytics import YOLO
    except ImportError:
        raise SystemExit('先安装: pip3 install ultralytics')

    model = YOLO(args.model)
    if args.pretrained:
        exported = model.export(format='onnx', imgsz=args.imgsz, opset=12, simplify=True)
        print(f'预训练 ONNX: {exported}')
        print('拷到车上: ~/models/yolov8n.onnx  然后 use_coco_names:=true')
        return

    if not os.path.isfile(args.data):
        raise SystemExit(f'数据集不存在: {args.data}  先运行 prepare_dataset 并标注')
    model.train(
        data=args.data,
        epochs=args.epochs,
        imgsz=args.imgsz,
        batch=args.batch,
        device=args.device or 'cpu',
        project=args.project,
        name=args.name,
    )
    best = os.path.join(args.project, args.name, 'weights', 'best.pt')
    exported = YOLO(best).export(format='onnx', imgsz=args.imgsz, opset=12, simplify=True)
    print(f'自定义 ONNX: {exported}')
    print('车上启动时 use_coco_names:=false, imgsz 必须和这里一致')


if __name__ == '__main__':
    main()
