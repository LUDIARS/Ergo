import { emitParticle, advanceParticles } from "./particle-simulation.js";
import { drawParticles } from "./particle-draw.js";
export { emitParticle, advanceParticles, sampleDiscPosition } from "./particle-simulation.js";
export { drawParticles } from "./particle-draw.js";

// Embeddable particle preview — the particle editor's emit / step / draw
// math shared with ui/index.html, packaged as an ES module taking config from
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

  const pv = {
    setConfig(next) { cfg = next; particles = []; emitAccum = 0; },
    dispose() { previews.delete(pv); },
    _step(dt) {
      emitAccum += cfg.emission.rate * dt;
      while (emitAccum >= 1) { emitParticle(cfg, particles); emitAccum -= 1; }
      advanceParticles(cfg, particles, dt);
    },
    _draw() {
      const W = canvas.width, H = canvas.height;
      ctx.setTransform(1, 0, 0, 1, 0, 0);
      ctx.globalCompositeOperation = "source-over";
      ctx.fillStyle = "#101216";
      ctx.fillRect(0, 0, W, H);
      drawParticles(canvas, ctx, cfg, particles, scale);
    },
  };

  previews.add(pv);
  if (!rafId) { lastFrame = performance.now(); rafId = requestAnimationFrame(tick); }
  return pv;
}
