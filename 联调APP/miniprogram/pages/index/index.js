const proto = require('../../utils/protocol.js')
const { drawCar } = require('../../utils/render.js')

const SMOOTH = 0.16
const FRAME_MS = 33

const num = (v) => (typeof v === 'number' && isFinite(v)) ? v : 0

Page({
  data: {
    connected: false,
    streaming: false,
    statusText: '未连接',
    host: proto.DEFAULT_HOST,
    port: String(proto.DEFAULT_PORT),
    freq: proto.DEFAULT_FREQ,
    freqText: proto.DEFAULT_FREQ + ' Hz',
    yawText: '--',
    speedText: '--',
    angleText: '--',
    voltText: '--',
    percentText: '--',
    batteryColor: '#3ddc84'
  },

  onLoad() {
    this.sock = null
    this.last = { yaw: 0, speed: 0, angle: 0, voltage: 0, percent: 0 }
    this.curYaw = 0
  },

  onReady() {
    this._initCanvas()
  },

  onUnload() {
    this._stopLoop()
    if (this.sock) this.sock.close()
  },

  onHostInput(e) { this.setData({ host: e.detail.value }) },
  onPortInput(e) { this.setData({ port: e.detail.value }) },

  onFreqChanging(e) {
    this.setData({ freq: e.detail.value, freqText: e.detail.value + ' Hz' })
  },
  onFreqChange(e) {
    this.setData({ freq: e.detail.value, freqText: e.detail.value + ' Hz' })
  },

  connect() {
    const host = (this.data.host || '').trim().replace(/^ws:\/\//, '')
    const port = parseInt(this.data.port, 10) || proto.DEFAULT_PORT
    const url = 'ws://' + host + ':' + port
    this.setData({ statusText: '连接 ' + url + ' ...' })
    const sock = wx.connectSocket({ url })
    sock.onOpen(() => {
      this.sock = sock
      this.setData({ connected: true, statusText: '已连接 Rock4' }, () => this._initCanvas())
      this._send(proto.buildPing())
    })
    sock.onMessage((e) => this._onMessage(e.data))
    sock.onClose(() => this._onClose())
    sock.onError(() => {
      if (!this.data.connected) this.setData({ statusText: '连接失败，请检查地址/端口' })
    })
  },

  disconnect() {
    if (this.sock) this.sock.close()
    this.sock = null
    this.setData({ connected: false, streaming: false, statusText: '已断开' })
  },

  onStreamToggle() {
    if (this.data.streaming) {
      this._send(proto.buildStop())
      this.setData({ streaming: false, statusText: '已停止数据流' })
    } else {
      this._send(proto.buildStart(this.data.freq))
      this.setData({ statusText: '请求数据流 ' + this.data.freq + ' Hz ...' })
    }
  },

  _send(str) { if (this.sock) this.sock.send({ data: str }) },

  _onMessage(raw) {
    const msg = proto.parse(raw)
    if (!msg) return
    switch (msg.type) {
      case 'hello':
        this.setData({ statusText: '已连接 · STM32 ' + (msg.stm32 || 'unknown') })
        break
      case 'ack':
        if (msg.ok && msg.cmd === 'start') {
          this.setData({ streaming: true, statusText: '数据流 ' + msg.freq + ' Hz' })
        } else if (msg.ok && msg.cmd === 'stop') {
          this.setData({ streaming: false, statusText: '已停止数据流' })
        } else {
          this.setData({ statusText: '指令被拒绝: ' + (msg.msg || '') })
        }
        break
      case 'data':
        this._onData(msg)
        break
      case 'error':
        this.setData({ statusText: '错误: ' + (msg.msg || '') })
        break
    }
  },

  _onData(m) {
    this.last = {
      yaw: num(m.yaw),
      speed: num(m.speed),
      angle: num(m.angle),
      voltage: num(m.voltage),
      percent: num(m.percent)
    }
    this.setData({
      yawText: this.last.yaw.toFixed(1),
      speedText: this.last.speed.toFixed(2),
      angleText: this.last.angle.toFixed(1),
      voltText: this.last.voltage.toFixed(2),
      percentText: String(Math.round(this.last.percent)),
      batteryColor: this.last.percent > 30 ? '#3ddc84' : '#ff5c5c'
    })
  },

  _onClose() {
    this.sock = null
    this.setData({ connected: false, streaming: false, statusText: '连接已断开' })
  },

  _initCanvas(attempt) {
    const n = attempt || 0
    wx.createSelectorQuery()
      .in(this)
      .select('#car')
      .fields({ node: true, size: true })
      .exec((res) => {
        const info = res && res[0]
        if (!info || !info.node) {
          if (n < 5) setTimeout(() => this._initCanvas(n + 1), 120)
          return
        }
        const canvas = info.node
        const ctx = canvas.getContext('2d')
        const dpr = (wx.getSystemInfoSync().pixelRatio) || 1
        canvas.width = info.width * dpr
        canvas.height = info.height * dpr
        ctx.scale(dpr, dpr)
        this.canvas = { ctx, w: info.width, h: info.height }
        this._startLoop()
      })
  },

  _startLoop() {
    if (this._timer) return
    this._timer = setInterval(() => this._frame(), FRAME_MS)
  },
  _stopLoop() {
    if (this._timer) { clearInterval(this._timer); this._timer = null }
  },

  _frame() {
    const c = this.canvas
    if (!c) return
    const tgt = this.data.connected ? this.last.yaw : 0
    let diff = ((tgt - this.curYaw + 540) % 360) - 180
    this.curYaw += diff * SMOOTH
    drawCar(c.ctx, c.w, c.h, this.curYaw, this.last.percent)
  }
})
