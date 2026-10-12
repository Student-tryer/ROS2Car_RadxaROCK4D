#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Rock4 模拟桥接服务（Windows 本地无硬件联调用）
- 监听 0.0.0.0:8765，实现 RFC6455 WebSocket 服务端（纯标准库，零依赖）
- 模拟 STM32 遥测：yaw/pitch/roll、速度、转向角、电池
- 手机小程序通过 ws://<本机局域网IP>:8765 连接
- 协议见 docs/protocol.md

用法:
    python server/mock_rock4d.py            # 默认 8765
    python server/mock_rock4d.py 9000       # 指定端口

真机运行时，此文件可作为 Rock4 侧桥接服务骨架：
把 SimulatedSTM32.read() 换成通过 pyserial 读写真实 STM32，其余（WebSocket
帧解析、指令分发、频率控制）逻辑保持不变。
"""
import base64
import hashlib
import json
import math
import random
import socket
import struct
import sys
import threading
import time

MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
MIN_FREQ, MAX_FREQ = 1, 200


def _ws_accept(key):
    return base64.b64encode(
        hashlib.sha1((key + MAGIC).encode()).digest()
    ).decode()


def _handshake(conn, data):
    headers = {}
    for line in data.split(b"\r\n")[1:]:
        if b":" in line:
            k, _, v = line.partition(b":")
            headers[k.strip().lower()] = v.strip()
    key = headers.get(b"sec-websocket-key", b"").decode()
    accept = _ws_accept(key)
    resp = (
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n\r\n"
    ).encode()
    conn.sendall(resp)


def _send_text(conn, text):
    payload = text.encode("utf-8")
    header = bytearray([0x81])  # FIN + text opcode
    n = len(payload)
    if n < 126:
        header.append(n)
    elif n < 65536:
        header.append(126)
        header += struct.pack(">H", n)
    else:
        header.append(127)
        header += struct.pack(">Q", n)
    conn.sendall(bytes(header) + payload)


def _recv(conn, size):
    data = b""
    while len(data) < size:
        chunk = conn.recv(size - len(data))
        if not chunk:
            return None
        data += chunk
    return data


def _recv_frame(conn):
    head = _recv(conn, 2)
    if head is None:
        return None
    opcode = head[0] & 0x0F
    masked = head[1] & 0x80
    length = head[1] & 0x7F
    if length == 126:
        b = _recv(conn, 2)
        if b is None:
            return None
        length = struct.unpack(">H", b)[0]
    elif length == 127:
        b = _recv(conn, 8)
        if b is None:
            return None
        length = struct.unpack(">Q", b)[0]
    mask = _recv(conn, 4) if masked else None
    payload = _recv(conn, length) if length else b""
    if payload is None:
        return None
    if masked and payload:
        payload = bytes(payload[i] ^ mask[i % 4] for i in range(len(payload)))
    return opcode, payload


class SimulatedSTM32:
    """模拟 STM32 传感器 + 电机反馈。真机时替换为串口读写。"""
    def __init__(self):
        self.yaw = random.uniform(0, 360)
        self.voltage = 7.62

    def read(self, streaming):
        now_ms = int(time.time() * 1000)
        turn = math.sin(now_ms / 2800.0)  # 缓慢转向分量
        self.yaw = (self.yaw + (turn * 5.0 + random.gauss(0, 1.1)) * 0.02) % 360.0
        speed = max(0.0, 0.55 + turn * 0.15 + random.gauss(0, 0.03))
        if streaming:
            self.voltage = max(6.2, self.voltage - 0.00004)
        pct = max(0.0, min(100.0, (self.voltage - 6.0) / 2.4 * 100.0))
        return {
            "type": "data",
            "t": now_ms,
            "yaw": round(self.yaw, 1),
            "pitch": round(random.gauss(0.2, 0.3), 2),
            "roll": round(random.gauss(-0.1, 0.3), 2),
            "speed": round(speed, 3),
            "angle": round(turn * 26.0, 1),
            "voltage": round(self.voltage, 2),
            "percent": round(pct),
            "rpm": round(speed * 1000.0),
        }


class Client(threading.Thread):
    def __init__(self, conn, addr):
        super().__init__(daemon=True)
        self.conn = conn
        self.addr = addr
        self.streaming = False
        self.freq = 20
        self.stop = threading.Event()
        self.sim = SimulatedSTM32()

    def send(self, obj):
        try:
            _send_text(self.conn, json.dumps(obj, ensure_ascii=False))
        except OSError:
            self.stop.set()

    def hello(self):
        self.send({
            "type": "hello",
            "stm32": "online",
            "fw": "mock-0.1",
            "freq_min": MIN_FREQ,
            "freq_max": MAX_FREQ,
        })

    def on_cmd(self, msg):
        cmd = msg.get("cmd")
        if cmd == "start":
            self.freq = min(MAX_FREQ, max(MIN_FREQ, int(msg.get("freq", 20))))
            self.streaming = True
            self.send({"type": "ack", "cmd": "start", "ok": True, "freq": self.freq})
        elif cmd == "stop":
            self.streaming = False
            self.send({"type": "ack", "cmd": "stop", "ok": True})
        elif cmd == "ping":
            self.hello()
        else:
            self.send({"type": "error", "msg": "unknown cmd: %s" % cmd})

    def run(self):
        try:
            first = self.conn.recv(4096)  # 读取 HTTP 升级请求
            if not first:
                return
            _handshake(self.conn, first)
            print("[conn] %s 已连接" % (self.addr,))
            self.hello()
            threading.Thread(target=self.telemetry, daemon=True).start()
            while not self.stop.is_set():
                frame = _recv_frame(self.conn)
                if frame is None:
                    break
                opcode, payload = frame
                if opcode == 0x8:  # close
                    break
                if opcode == 0x9:  # ping -> pong
                    try:
                        self.conn.sendall(bytes([0x8A, len(payload)]) + payload)
                    except OSError:
                        break
                    continue
                if opcode == 0x1 and payload:  # text
                    try:
                        self.on_cmd(json.loads(payload.decode("utf-8")))
                    except (ValueError, KeyError):
                        self.send({"type": "error", "msg": "bad json"})
        except OSError:
            pass
        finally:
            self.stop.set()
            try:
                self.conn.close()
            except OSError:
                pass
            print("[conn] %s 断开" % (self.addr,))

    def telemetry(self):
        while not self.stop.is_set():
            if self.streaming:
                self.send(self.sim.read(True))
                time.sleep(1.0 / self.freq)
            else:
                self.sim.read(False)
                time.sleep(0.2)


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", port))
    srv.listen(8)
    print("Rock4 模拟服务监听 0.0.0.0:%d（小程序填本机局域网 IP）" % port)
    while True:
        conn, addr = srv.accept()
        Client(conn, addr).start()


if __name__ == "__main__":
    main()
