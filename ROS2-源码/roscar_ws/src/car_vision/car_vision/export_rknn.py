import argparse
import os


def main():
    parser = argparse.ArgumentParser(description='在 x86 PC 上把 ONNX 转成 RK3576 RKNN, 不要在车上跑')
    parser.add_argument('--onnx', required=True)
    parser.add_argument('--output', default='')
    parser.add_argument('--platform', default='rk3576')
    parser.add_argument('--dataset', default='', help='量化图片列表, 每行一个 jpg 路径')
    args = parser.parse_args()
    try:
        from rknn.api import RKNN
    except ImportError:
        raise SystemExit(
            'rknn.api 只在转换用的 PC 上. 安装 airockchip/rknn-toolkit2, 不要装到 RK3576 板上')

    out = args.output or os.path.splitext(args.onnx)[0] + '.rknn'
    rknn = RKNN(verbose=True)
    rknn.config(
        mean_values=[[0, 0, 0]],
        std_values=[[255, 255, 255]],
        target_platform=args.platform,
    )
    if rknn.load_onnx(model=args.onnx) != 0:
        raise SystemExit('load_onnx 失败')
    if args.dataset:
        ret = rknn.build(do_quantization=True, dataset=args.dataset)
    else:
        ret = rknn.build(do_quantization=False)
    if ret != 0:
        raise SystemExit('build 失败')
    if rknn.export_rknn(out) != 0:
        raise SystemExit('export 失败')
    rknn.release()
    print(f'已导出 {out}')
    print('拷到车上 ~/models/ 后: ros2 run car_vision yolo_detect --ros-args -p rknn_path:=~/models/xxx.rknn')


if __name__ == '__main__':
    main()
