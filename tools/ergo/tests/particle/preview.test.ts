import { test } from "node:test";
import assert from "node:assert/strict";
import { DEFAULT_EFFECT } from "../../src/plugins/particle/schema.js";
// Browser modules are shipped as plain JavaScript, without declaration files.
// @ts-ignore
import { emitParticle, advanceParticles, drawParticles, createPreview } from "../../src/plugins/particle/ui/preview.js";

function config() {
  const cfg = structuredClone(DEFAULT_EFFECT);
  cfg.emission.maxAlive = 1;
  cfg.initial.speedMin = cfg.initial.speedMax = 4;
  cfg.initial.velocityAngleDeg = cfg.initial.velocityAngleSpreadDeg = 0;
  cfg.initial.lifetimeMin = cfg.initial.lifetimeMax = 2;
  cfg.overLife.velocityDamping = 0.25;
  cfg.forces.gravity = [2, -4];
  return cfg;
}

test("editor emitter uses the supplied SVG point and never samples above the cap", () => {
  const cfg = config();
  const particles: any[] = [];
  let samples = 0;
  const point = () => { samples++; return [7, 9]; };
  emitParticle(cfg, particles, point);
  emitParticle(cfg, particles, point);
  assert.equal(samples, 1);
  assert.deepEqual(particles[0], { x: 7, y: 9, vx: 4, vy: 0, lifetime: 2, age: 0, initSize: cfg.initial.size });
  advanceParticles(cfg, particles, 0.5);
  assert.equal(particles[0].vx, 3);
  assert.equal(particles[0].vy, -2);
  assert.equal(particles[0].x, 8.5);
  assert.equal(particles[0].y, 8);
  advanceParticles(cfg, particles, 1.5);
  assert.equal(particles.length, 0);
});

function context() {
  const calls: { kind: string; args: number[]; color: string; blend: string }[] = [];
  const ctx = {
    globalCompositeOperation: "source-over", fillStyle: "",
    setTransform() {}, beginPath() {}, fill() {},
    fillRect(...args: number[]) { calls.push({ kind: "rect", args, color: this.fillStyle, blend: this.globalCompositeOperation }); },
    arc(...args: number[]) { calls.push({ kind: "arc", args, color: this.fillStyle, blend: this.globalCompositeOperation }); },
  };
  return { ctx, calls };
}

test("drawing preserves scale, interpolation, shape, additive blend and small-particle cutoff", () => {
  const cfg = config();
  cfg.render.shape = "square";
  cfg.render.blend = "additive";
  cfg.overLife.sizeStart = 1; cfg.overLife.sizeEnd = 0;
  cfg.overLife.colorStart = [1, 0, 0, 1]; cfg.overLife.colorEnd = [0, 0, 1, 0];
  const particles = [{ x: 3, y: -2, age: 1, lifetime: 2, initSize: 4 }];
  const { ctx, calls } = context();
  drawParticles({ width: 100, height: 80 }, ctx, cfg, particles, 2);
  assert.deepEqual(calls, [{ kind: "rect", args: [54, 34, 4, 4], color: "rgba(128,0,128,0.5)", blend: "lighter" }]);
  assert.equal(ctx.globalCompositeOperation, "source-over");
  cfg.render.shape = "circle";
  drawParticles({ width: 100, height: 80 }, ctx, cfg, particles, 1);
  assert.deepEqual(calls[1].args, [53, 38, 1, 0, Math.PI * 2]);
  drawParticles({ width: 100, height: 80 }, ctx, cfg, particles, 0.25);
  assert.equal(calls.length, 2);
});

test("embedded previews reset on config replacement, use disc fallback, and stop after dispose", (t) => {
  let queued: (now: number) => void = () => {};
  const descriptor = Object.getOwnPropertyDescriptor(globalThis, "requestAnimationFrame");
  Object.defineProperty(globalThis, "requestAnimationFrame", { configurable: true, value: (fn: (now: number) => void) => { queued = fn; return 1; } });
  t.after(() => { if (descriptor) Object.defineProperty(globalThis, "requestAnimationFrame", descriptor); else delete (globalThis as any).requestAnimationFrame; });
  const cfg = config();
  cfg.initial.positionShape = "svgFill";
  cfg.initial.positionRadius = 0;
  cfg.initial.size = 8;
  cfg.emission.rate = 100;
  const { ctx, calls } = context();
  const pv = createPreview({ width: 100, height: 80, getContext: () => ctx }, cfg);
  t.after(() => { pv.dispose(); queued(performance.now()); });
  pv._step(0.02); pv._draw();
  assert.ok(calls.length > 1, "background and a fallback particle are drawn");
  pv.setConfig(cfg); calls.length = 0; pv._draw();
  assert.equal(calls.length, 1, "config replacement clears particles");
  pv.dispose(); calls.length = 0; queued(performance.now());
  assert.equal(calls.length, 0, "disposed preview receives no frames");
});
