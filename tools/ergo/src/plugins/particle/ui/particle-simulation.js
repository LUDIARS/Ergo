// Shared integration; the caller owns particles and the emission clock.
function rand(a, b) { return a + Math.random() * (b - a); }

export function sampleDiscPosition(cfg) {
  const a = Math.random() * Math.PI * 2;
  const r = Math.sqrt(Math.random()) * cfg.initial.positionRadius;
  return [Math.cos(a) * r, Math.sin(a) * r];
}

export function emitParticle(cfg, particles, samplePosition = () => sampleDiscPosition(cfg)) {
  if (particles.length >= cfg.emission.maxAlive) return;
  const [x, y] = samplePosition();
  const ang = (cfg.initial.velocityAngleDeg + rand(-1, 1) * cfg.initial.velocityAngleSpreadDeg/2) * Math.PI / 180;
  const speed = rand(cfg.initial.speedMin, cfg.initial.speedMax);
  const p = { x, y, vx: Math.cos(ang) * speed, vy: Math.sin(ang) * speed,
    lifetime: rand(cfg.initial.lifetimeMin, cfg.initial.lifetimeMax), age: 0, initSize: cfg.initial.size };
  particles.push(p);
  return p;
}

export function advanceParticles(cfg, particles, dt) {
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
}
