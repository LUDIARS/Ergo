// Pins sanitize's clamping after its ranges moved to particle/ranges.ts.

import { test } from "node:test";
import assert from "node:assert/strict";

import { DEFAULT_EFFECT } from "../../src/plugins/particle/schema.js";
import { sanitizePresets } from "../../src/plugins/vfx-suggest/sanitize.js";

const one = (config: object) => sanitizePresets([{ name: "x", config }])[0]!.config;

test("sanitize clamps to the same bounds as before", () => {
    const hi = one({
        emission: { rate: 1e6, maxAlive: 1e6 },
        initial: { positionRadius: 1e6, velocityAngleDeg: 1e6, velocityAngleSpreadDeg: 1e6,
                   speedMin: 1e6, speedMax: 1e6, lifetimeMin: 1e6, lifetimeMax: 1e6, size: 1e6,
                   color: [9, 9, 9, 9] },
        overLife: { sizeStart: 1e6, sizeEnd: 1e6, velocityDamping: 1e6 },
        forces: { gravity: [1e6, -1e6] },
    });
    assert.deepEqual(hi.emission, { rate: 400, maxAlive: 2000 });
    assert.equal(hi.initial.positionRadius, 256);
    assert.equal(hi.initial.velocityAngleDeg, 360);
    assert.equal(hi.initial.velocityAngleSpreadDeg, 360);
    assert.equal(hi.initial.speedMax, 600);
    assert.equal(hi.initial.lifetimeMax, 6);
    assert.equal(hi.initial.size, 64);
    assert.deepEqual(hi.initial.color, [1, 1, 1, 1]);
    assert.equal(hi.overLife.sizeStart, 8);
    assert.equal(hi.overLife.velocityDamping, 1);
    assert.deepEqual(hi.forces.gravity, [600, -600]);

    const lo = one({ emission: { rate: -1 }, initial: { lifetimeMin: 0 } });
    assert.equal(lo.emission.rate, 1);
    assert.equal(lo.initial.lifetimeMin, 0.05);
});

test("sanitize keeps its defaults for missing fields", () => {
    const cfg = one({});
    assert.equal(cfg.emission.rate, DEFAULT_EFFECT.emission.rate);
    assert.equal(cfg.overLife.sizeStart, 1);
    assert.equal(cfg.overLife.sizeEnd, 0);
    assert.equal(cfg.initial.positionShape, "disc");
    assert.equal(cfg.render.blend, "additive");
});
