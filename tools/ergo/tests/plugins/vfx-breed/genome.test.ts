import { test } from "node:test";
import assert from "node:assert/strict";

import { DEFAULT_EFFECT, type ParticleEffectConfig } from "../../../src/plugins/particle/schema.js";
import { PARTICLE_RANGES } from "../../../src/plugins/particle/ranges.js";
import { numericValues, assertWithinRanges } from "./helpers.js";
import { sanitizePresets } from "../../../src/plugins/vfx-suggest/sanitize.js";
import { GENES, geneIndex } from "../../../src/plugins/vfx-breed/genes.js";
import { encode, decode, fold } from "../../../src/plugins/vfx-breed/genome.js";
import { rgbaToHsva, hsvaToRgba } from "../../../src/plugins/vfx-breed/color.js";

test("every numeric gene takes its range from the shared table", () => {
    for (const g of GENES) {
        if (g.source.kind === "number") assert.deepEqual(g.range, PARTICLE_RANGES[g.source.key]);
    }
    assert.equal(Object.keys(PARTICLE_RANGES).length, GENES.filter((g) => g.source.kind === "number").length);
});

test("encode then decode reproduces the default effect", () => {
    const back = decode(encode(DEFAULT_EFFECT), DEFAULT_EFFECT, DEFAULT_EFFECT.name);
    const a = numericValues(DEFAULT_EFFECT);
    const b = numericValues(back);
    for (const k of Object.keys(a)) assert.ok(Math.abs(a[k]! - b[k]!) < 0.01, `${k}: ${a[k]} vs ${b[k]}`);
    DEFAULT_EFFECT.initial.color.forEach((x, i) => assert.ok(Math.abs(x - back.initial.color[i]!) < 0.002));
    assert.equal(back.render.blend, DEFAULT_EFFECT.render.blend);
    assert.equal(back.render.shape, DEFAULT_EFFECT.render.shape);
});

test("every gene is normalised into 0..1", () => {
    const wild = {
        emission: { rate: 9999, maxAlive: -5 },
        initial: { speedMin: 1e6, size: -1, color: [2, -1, 0.5, 3] },
        forces: { gravity: [-1e4, 1e4] },
    };
    for (const g of encode(wild).genes) assert.ok(g >= 0 && g <= 1);
});

test("decode clamps to the same ranges vfx-suggest's sanitize uses", () => {
    const extremes = [0, 1].map((v) => ({ genes: GENES.map(() => v), blend: "alpha" as const, shape: "square" as const }));
    for (const genome of extremes) {
        const cfg = decode(genome, DEFAULT_EFFECT, "x");
        assertWithinRanges(cfg);
        // sanitize leaves an in-range config's numbers untouched.
        const sane = sanitizePresets([{ name: "x", config: cfg }])[0]!.config;
        assert.deepEqual(numericValues(sane), numericValues(cfg));
    }
});

test("decode orders min/max pairs and rounds integer genes", () => {
    const genome = encode(DEFAULT_EFFECT);
    genome.genes[geneIndex("initial.speedMin")] = 0.9;
    genome.genes[geneIndex("initial.speedMax")] = 0.1;
    genome.genes[geneIndex("emission.maxAlive")] = 0.537;
    const cfg = decode(genome, DEFAULT_EFFECT, "x");
    assert.ok(cfg.initial.speedMin <= cfg.initial.speedMax);
    assert.ok(Number.isInteger(cfg.emission.maxAlive));
});

test("positionShape / svgSource / svgScale come from the carrier, not the genome", () => {
    const carrier = {
        ...DEFAULT_EFFECT,
        initial: { ...DEFAULT_EFFECT.initial, positionShape: "svgStroke", svgSource: "M0 0 L10 10", svgScale: 2.5 },
    };
    const cfg = decode(encode(DEFAULT_EFFECT), carrier, "x");
    assert.equal(cfg.initial.positionShape, "svgStroke");
    assert.equal(cfg.initial.svgSource, "M0 0 L10 10");
    assert.equal(cfg.initial.svgScale, 2.5);
});

test("hue wraps, other genes clamp", () => {
    assert.ok(Math.abs(fold(1.25, true) - 0.25) < 1e-9);
    assert.ok(Math.abs(fold(-0.25, true) - 0.75) < 1e-9);
    assert.equal(fold(1.25, false), 1);
    assert.equal(fold(-0.25, false), 0);
});

test("HSV round trip", () => {
    for (const c of [[1, 0.75, 0.25, 1], [0.2, 0.4, 0.9, 0.5], [0, 0, 0, 0], [1, 1, 1, 1]] as const) {
        const back = hsvaToRgba(rgbaToHsva([...c]));
        c.forEach((x, i) => assert.ok(Math.abs(x - back[i]!) < 1e-9));
    }
});
