// Embeddable particle preview — the particle editor's emit / step / draw
// math (ui/index.html) packaged as an ES module that takes the config from
// the caller, so other plugin UIs (vfx-breed) can show many small previews.
//
// Served at /particle/preview.js. Spawn positions use the disc shape;
// svgStroke / svgFill fall back to disc here, as they do in ergo_particle.
//
//   import { createPreview } from "/particle/preview.js";
//   const pv = createPreview(canvas, config, { scale: 0.5 });
//   pv.setConfig(next);  pv.dispose();

const previews = new Set();
let lastFrame = 0;
let rafId = 0;

function rand(a, b) { return a + Math.random() * (b - a); }
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

function tick(now) {
  const dt = Math.min(0.05, (now - lastFrame) / 1000);
  lastFrame = now;
  for (const pv of previews) { pv._step(dt); pv._draw(); }
  rafId = previews.size ? requestAnimationFrame(tick) : 0;
}

/**
 * @param {HTMLCanvasElement} canvas
 * @param {object} config  ParticleEffectConfig
 * @param {{ scale?: number }} [opts]  world -> canvas scale (1 = editor size)
 */
export function createPreview(canvas, config, opts = {}) {
  const ctx = canvas.getContext("2d");
  const scale = opts.scale ?? 1;
  let cfg = config;
  let particles = [];
  let emitAccum = 0;

  function emit() {
    if (particles.length >= cfg.emission.maxAlive) return;
    const a = Math.random() * Math.PI * 2;
    const r = Math.sqrt(Math.random()) * cfg.initial.positionRadius;
    const ang = (cfg.initial.velocityAngleDeg + rand(-1, 1) * cfg.initial.velocityAngleSpreadDeg/2) * Math.PI / 180;
    const speed = rand(cfg.initial.speedMin, cfg.initial.speedMax);
    particles.push({
      x: Math.cos(a) * r, y: Math.sin(a) * r,
      vx: Math.cos(ang) * speed, vy: Math.sin(ang) * speed,
      lifetime: rand(cfg.initial.lifetimeMin, cfg.initial.lifetimeMax),
      age: 0,
      initSize: cfg.initial.size,
    });
  }

  const pv = {
    setConfig(next) { cfg = next; particles = []; emitAccum = 0; },
    dispose() { previews.delete(pv); },
    _step(dt) {
      emitAccum += cfg.emission.rate * dt;
      while (emitAccum >= 1) { emit(); emitAccum -= 1; }
      const damp = Math.pow(cfg.overLife.velocityDamping, dt);
      const gx = cfg.forces.gravity[0], gy = cfg.forces.gravity[1];
      for (let i = particles.length - 1; i >= 0; --i) {
        const p = particles[i];
        p.age += dt;
        if (p.age >= p.lifetime) { particles.splice(i, 1); continue; }
        p.vx = p.vx * damp + gx * dt;
        p.vy = p.vy * damp + gy * dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
      }
    },
    _draw() {
      const W = canvas.width, H = canvas.height;
      ctx.setTransform(1, 0, 0, 1, 0, 0);
      ctx.globalCompositeOperation = "source-over";
      ctx.fillStyle = "#101216";
      ctx.fillRect(0, 0, W, H);
      const ox = W/2, oy = H/2;
      ctx.globalCompositeOperation = (cfg.render.blend === "additive") ? "lighter" : "source-over";
      for (const p of particles) {
        const t = Math.min(1, Math.max(0, p.age / p.lifetime));
        const size = p.initSize * lerp(cfg.overLife.sizeStart, cfg.overLife.sizeEnd, t) * scale;
        if (size <= 0.5) continue;
        ctx.fillStyle = colorToRgba(lerpColor(cfg.overLife.colorStart, cfg.overLife.colorEnd, t));
        const x = ox + p.x * scale, y = oy + p.y * scale;
        if (cfg.render.shape === "square") {
          ctx.fillRect(x - size/2, y - size/2, size, size);
        } else {
          ctx.beginPath();
          ctx.arc(x, y, size/2, 0, Math.PI*2);
          ctx.fill();
        }
      }
      ctx.globalCompositeOperation = "source-over";
    },
  };

  previews.add(pv);
  if (!rafId) { lastFrame = performance.now(); rafId = requestAnimationFrame(tick); }
  return pv;
}
