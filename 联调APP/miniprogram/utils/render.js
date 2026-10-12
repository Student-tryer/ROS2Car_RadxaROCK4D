// 中心小车朝向渲染 + 指南针 + 电池图标（2D canvas）

const THEME = {
  ring: '#2a3150',
  tick: '#3a4264',
  tickBold: '#5a6491',
  label: '#6b7399',
  body: '#e9edf6',
  bodyStroke: '#9aa3c4',
  wheel: '#141827',
  arrow: '#4c8dff',
  battery: '#3a4264',
  batteryFill: '#3ddc84',
  batteryFillLow: '#ff5c5c'
}

function rr(ctx, x, y, w, h, r) {
  ctx.beginPath()
  ctx.moveTo(x + r, y)
  ctx.arcTo(x + w, y, x + w, y + h, r)
  ctx.arcTo(x + w, y + h, x, y + h, r)
  ctx.arcTo(x, y + h, x, y, r)
  ctx.arcTo(x, y, x + w, y, r)
  ctx.closePath()
}

function drawCar(ctx, w, h, yawDeg, percent) {
  const cx = w / 2
  const cy = h / 2
  ctx.clearRect(0, 0, w, h)
  drawCompass(ctx, cx, cy)
  drawBattery(ctx, w, h, percent)
  drawVehicle(ctx, cx, cy, yawDeg)
}

function drawCompass(ctx, cx, cy) {
  const r = Math.min(cx, cy) - 10
  ctx.save()
  ctx.strokeStyle = THEME.ring
  ctx.lineWidth = 2
  ctx.beginPath()
  ctx.arc(cx, cy, r, 0, Math.PI * 2)
  ctx.stroke()
  for (let i = 0; i < 360; i += 15) {
    const big = i % 90 === 0
    const a = (i - 90) * Math.PI / 180
    const x0 = cx + Math.cos(a) * (r - 4)
    const y0 = cy + Math.sin(a) * (r - 4)
    const x1 = cx + Math.cos(a) * (r - (big ? 14 : 9))
    const y1 = cy + Math.sin(a) * (r - (big ? 14 : 9))
    ctx.strokeStyle = big ? THEME.tickBold : THEME.tick
    ctx.lineWidth = big ? 2.5 : 1.5
    ctx.beginPath()
    ctx.moveTo(x0, y0)
    ctx.lineTo(x1, y1)
    ctx.stroke()
  }
  ctx.fillStyle = THEME.label
  ctx.font = 'bold 14px sans-serif'
  ctx.textAlign = 'center'
  ctx.textBaseline = 'middle'
  ctx.fillText('N', cx, cy - r + 20)
  ctx.restore()
}

function drawBattery(ctx, w, h, percent) {
  const bw = 64, bh = 26
  const x = w - bw - 14, y = 14
  const pct = Math.max(0, Math.min(100, Number(percent) || 0))
  ctx.save()
  ctx.strokeStyle = THEME.battery
  ctx.lineWidth = 2
  ctx.fillStyle = THEME.battery
  ctx.fillRect(x + bw, y + bh / 2 - 5, 5, 10)
  rr(ctx, x, y, bw, bh, 4)
  ctx.stroke()
  const fillW = (bw - 4) * pct / 100
  if (fillW > 1) {
    ctx.fillStyle = pct > 30 ? THEME.batteryFill : THEME.batteryFillLow
    rr(ctx, x + 2, y + 2, fillW, bh - 4, 3)
    ctx.fill()
  }
  ctx.fillStyle = '#dfe4f5'
  ctx.font = 'bold 13px sans-serif'
  ctx.textAlign = 'right'
  ctx.textBaseline = 'middle'
  ctx.fillText(Math.round(pct) + '%', x - 8, y + bh / 2)
  ctx.restore()
}

function drawVehicle(ctx, cx, cy, yawDeg) {
  const rad = yawDeg * Math.PI / 180
  ctx.save()
  ctx.translate(cx, cy)
  ctx.rotate(rad)
  ctx.fillStyle = THEME.wheel
  const wheels = [[-34, -24, 16, 8], [18, -24, 16, 8], [-34, 16, 16, 8], [18, 16, 16, 8]]
  for (const w of wheels) ctx.fillRect(w[0], w[1], w[2], w[3])
  ctx.fillStyle = THEME.body
  ctx.strokeStyle = THEME.bodyStroke
  ctx.lineWidth = 2
  rr(ctx, -28, -16, 56, 32, 6)
  ctx.fill()
  ctx.stroke()
  ctx.fillStyle = THEME.arrow
  ctx.beginPath()
  ctx.moveTo(18, 0)
  ctx.lineTo(6, -9)
  ctx.lineTo(6, 9)
  ctx.closePath()
  ctx.fill()
  ctx.restore()
}

module.exports = { drawCar }
