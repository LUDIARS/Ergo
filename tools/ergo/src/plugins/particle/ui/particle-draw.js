// Shared particle drawing; callers own backgrounds, guides and scheduling.
function lerp(a, b, t) { return a + (b - a) * t; }
function lerpColor(a, b, t) {
  return [lerp(a[0],b[0],t), lerp(a[1],b[1],t), lerp(a[2],b[2],t), lerp(a[3],b[3],t)];
}
function colorToRgba(c) {
  const r = Math.round(Math.max(0, Math.min(1, c[0]))*255);
  const g = Math.round(Math.max(0, Math.min(1, c[1]))*255);
  const b = Math.round(Math.max(0, Math.min(1, c[2]))*255);
  return "rgba("+r+","+g+","+b+","+Math.max(0, Math.min(1, c[3]))+")";
}

export function drawParticles(canvas, ctx, cfg, particles, scale = 1) {
  const ox = canvas.width / 2, oy = canvas.height / 2;
  ctx.globalCompositeOperation = cfg.render.blend === "additive" ? "lighter" : "source-over";
  for (const p of particles) {
    const t = Math.min(1, Math.max(0, p.age / p.lifetime));
    const size = p.initSize * lerp(cfg.overLife.sizeStart, cfg.overLife.sizeEnd, t) * scale;
    if (size <= 0.5) continue;
    ctx.fillStyle = colorToRgba(lerpColor(cfg.overLife.colorStart, cfg.overLife.colorEnd, t));
    const x = ox + p.x * scale, y = oy + p.y * scale;
    if (cfg.render.shape === "square") ctx.fillRect(x - size/2, y - size/2, size, size);
    else { ctx.beginPath(); ctx.arc(x, y, size/2, 0, Math.PI*2); ctx.fill(); }
  }
  ctx.globalCompositeOperation = "source-over";
}
