import assert from "node:assert/strict";

import type { ParticleEffectConfig } from "../../../src/plugins/particle/schema.js";
import { PARTICLE_RANGES } from "../../../src/plugins/particle/ranges.js";

export function numericValues(cfg: ParticleEffectConfig): Record<string, number> {
    return {
        "emission.rate": cfg.emission.rate,
        "emission.maxAlive": cfg.emission.maxAlive,
        "initial.positionRadius": cfg.initial.positionRadius,
        "initial.velocityAngleDeg": cfg.initial.velocityAngleDeg,
        "initial.velocityAngleSpreadDeg": cfg.initial.velocityAngleSpreadDeg,
        "initial.speedMin": cfg.initial.speedMin,
        "initial.speedMax": cfg.initial.speedMax,
        "initial.lifetimeMin": cfg.initial.lifetimeMin,
        "initial.lifetimeMax": cfg.initial.lifetimeMax,
        "initial.size": cfg.initial.size,
        "overLife.sizeStart": cfg.overLife.sizeStart,
        "overLife.sizeEnd": cfg.overLife.sizeEnd,
        "overLife.velocityDamping": cfg.overLife.velocityDamping,
        "forces.gravity.x": cfg.forces.gravity[0],
        "forces.gravity.y": cfg.forces.gravity[1],
    };
}

export function assertWithinRanges(cfg: ParticleEffectConfig): void {
    for (const [k, v] of Object.entries(numericValues(cfg))) {
        const r = PARTICLE_RANGES[k as keyof typeof PARTICLE_RANGES];
        assert.ok(v >= r.lo && v <= r.hi, `${k}=${v} outside ${r.lo}..${r.hi}`);
    }
    for (const c of [cfg.initial.color, cfg.overLife.colorStart, cfg.overLife.colorEnd]) {
        for (const x of c) assert.ok(x >= 0 && x <= 1, `color channel ${x} outside 0..1`);
    }
    assert.ok(cfg.initial.speedMin <= cfg.initial.speedMax);
    assert.ok(cfg.initial.lifetimeMin <= cfg.initial.lifetimeMax);
}
