// ============================================================
// 协议中心文件 —— 手机 <-> Rock4 (WebSocket) 消息格式
// 协议集中在文件，改动只需改这里，详见 docs/protocol.md
// ============================================================

// 默认连接参数（小程序界面里可手动修改地址）
const DEFAULT_HOST = '192.168.1.100'
const DEFAULT_PORT = 8765
const DEFAULT_FREQ = 20          // 默认数据频率 Hz
const FREQ_MIN = 1
const FREQ_MAX = 200

function clampFreq(f) {
  const n = Math.round(Number(f) || DEFAULT_FREQ)
  return Math.min(FREQ_MAX, Math.max(FREQ_MIN, n))
}

// 手机 -> Rock4 指令
function buildStart(freq) {
  return JSON.stringify({ type: 'cmd', cmd: 'start', freq: clampFreq(freq) })
}
function buildStop() {
  return JSON.stringify({ type: 'cmd', cmd: 'stop' })
}
function buildPing() {
  return JSON.stringify({ type: 'cmd', cmd: 'ping' })
}

// Rock4 -> 手机 消息解析
function parse(raw) {
  try {
    const o = JSON.parse(raw)
    if (o && typeof o.type === 'string') return o
  } catch (e) { /* ignore */ }
  return null
}

module.exports = {
  DEFAULT_HOST, DEFAULT_PORT, DEFAULT_FREQ, FREQ_MIN, FREQ_MAX,
  buildStart, buildStop, buildPing, parse
}
