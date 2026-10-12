# ROS 小车通信协议 v0.1（草稿，可改）

> 现状：协议尚未最终确定。本文件是建议稿，改动只需同步更新
> `miniprogram/utils/protocol.js`、`server/mock_rock4d.py` 以及 Rock4 桥接/STM32 解析代码。

## 分层与链路

```
手机小程序 ──WebSocket(JSON)──> Rock4 桥接 ──UART(115200 8N1)──> STM32
     ◄────────────────────────┘    ◄───────────────────────────┘
```

- 手机 <-> Rock4：WebSocket，文本帧，UTF-8 JSON，局域网内 `ws://<Rock4IP>:8765`
- Rock4 <-> STM32：USART1（PB6/PB7），115200 8N1，二进制帧（见 B 节）
- 单位约定：频率 Hz，角度 °，速度 m/s，电压 V

## A. 手机 <-> Rock4（WebSocket / JSON）

### A1. 手机 -> Rock4（指令）
| type | cmd   | 参数   | 说明                                       |
|------|-------|--------|--------------------------------------------|
| cmd  | start | freq   | 让 Rock4 命令 STM32 以 freq Hz 回传数据     |
| cmd  | stop  | -      | 停止数据流                                 |
| cmd  | ping  | -      | 探活，Rock4 回 `hello`                     |

示例：
```json
{"type":"cmd","cmd":"start","freq":50}
```

### A2. Rock4 -> 手机
- `hello` 连接建立 / 探活响应：
  ```json
  {"type":"hello","stm32":"online","fw":"0.1","freq_min":1,"freq_max":200}
  ```
- `ack` 指令确认：
  ```json
  {"type":"ack","cmd":"start","ok":true,"freq":50}
  ```
- `data` 周期性遥测（频率由 start 指定）：
  ```json
  {
    "type":"data","t":1724671234567,
    "yaw":12.3,"pitch":0.2,"roll":-1.1,
    "speed":0.43,"angle":35.6,
    "voltage":7.61,"percent":76,"rpm":430
  }
  ```
- `error` 错误信息：`{"type":"error","msg":"..."}`

### A3. 朝向约定
- 0° = 屏幕上方（罗盘 N），逆时针为正（绕 Z 轴）。
- 手机以 `yaw` 旋转中心小车朝向。

## B. Rock4 <-> STM32（UART 二进制帧，建议）

沿用 `通讯协议.txt` 帧头约定：`AA 55 <ID> <LEN> <DATA...> <CRC>`
- ID：消息类型（建议 0x01=start, 0x02=stop, 0x10=遥测, 0x11=心跳）
- LEN：DATA 字节数
- CRC：1 字节，前 4 字节异或（或求和，待定）

你的示例 `AA 55 01 04 00 75 06 28 crc`
= 帧头 AA55 + ID 0x01 + LEN 0x04 + 数据 4 字节（0x0075=117, 0x0628=1576）。

> 说明：`通讯协议.txt` 目前只有这一条示例，ID 与字段的具体含义待你确认后再细化。
> STM32 当前 main.c 只做 TX 调试输出（yaw/电压），尚未有 RX 命令解析与频率控制，属下一步工作。

## C. 当前进度
- [x] 手机小程序（连接 + 实时仪表盘）
- [x] Rock4 侧模拟服务 `server/mock_rock4d.py`（无硬件可联调）
- [ ] STM32 接收 start/stop + 频率并按帧回传（下一步）
- [ ] Rock4 真实桥接（pyserial <-> WebSocket）
