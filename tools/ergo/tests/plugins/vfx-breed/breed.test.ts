import { test } from "node:test";
import assert from "node:assert/strict";

import { DEFAULT_EFFECT } from "../../../src/plugins/particle/schema.js";
import { CATALOG } from "../../../src/plugins/vfx-suggest/catalog.js";
import { GENES, geneIndex } from "../../../src/plugins/vfx-breed/genes.js";
import { encode } from "../../../src/plugins/vfx-breed/genome.js";
import { mutate, flipProbability, MAX_FLIP } from "../../../src/plugins/vfx-breed/mutate.js";
import { crossover } from "../../../src/plugins/vfx-breed/crossover.js";
import { breed } from "../../../src/plugins/vfx-breed/breed.js";
import { makeRng } from "../../../src/plugins/vfx-breed/rng.js";
import { assertWithinRanges } from "./helpers.js";

const zeroBias = GENES.map(() => 0);

test("rng: same seed, same sequence; different seed, different sequence", () => {
    const a = makeRng(42), b = makeRng(42), c = makeRng(43);
    const xs = Array.from({ length: 20 }, () => a.next());
    assert.deepEqual(xs, Array.from({ length: 20 }, () => b.next()));
    assert.notDeepEqual(xs, Array.from({ length: 20 }, () => c.next()));
    for (const x of xs) assert.ok(x >= 0 && x < 1);
});

test("mutate: strength 0 changes nothing", () => {
    const g = encode(DEFAULT_EFFECT);
    const m = mutate(g, 0, null, makeRng(1));
    assert.deepEqual(m, g);
});

test("mutate: genes move, stay in 0..1, and flips are rare", () => {
    const g = encode(DEFAULT_EFFECT);
    const rng = makeRng(7);
    let flips = 0;
    const N = 400;
    let moved = 0;
    for (let i = 0; i < N; ++i) {
        const m = mutate(g, 0.2, null, rng);
        for (const x of m.genes) assert.ok(x >= 0 && x <= 1);
        if (m.blend !== g.blend) flips++;
        if (m.genes.some((x, j) => x !== g.genes[j])) moved++;
    }
    assert.equal(moved, N);
    const rate = flips / N;
    assert.ok(rate > 0 && rate < 0.25, `flip rate ${rate}`);
    assert.ok(flipProbability(10) === MAX_FLIP);
});

test("mutate: bias shifts the mean of the moved gene", () => {
    const g = encode(DEFAULT_EFFECT);
    const i = geneIndex("initial.lifetimeMax");
    const up = [...zeroBias]; up[i] = 1;
    const down = [...zeroBias]; down[i] = -1;
    const mean = (bias: number[]) => {
        const rng = makeRng(99);
        let s = 0;
        for (let k = 0; k < 300; ++k) s += mutate(g, 0.1, bias, rng).genes[i]!;
        return s / 300;
    };
    const base = mean(zeroBias);
    assert.ok(mean(up) > base + 0.05, "positive weight raises the gene");
    assert.ok(mean(down) < base - 0.05, "negative weight lowers the gene");
});

test("crossover: every gene comes from one of the parents, both parents contribute", () => {
    const a = encode(CATALOG[0]!.config);
    const b = encode(CATALOG[1]!.config);
    const child = crossover(a, b, makeRng(5));
    let fromA = 0, fromB = 0;
    child.genes.forEach((x, i) => {
        assert.ok(x === a.genes[i] || x === b.genes[i]);
        if (x === a.genes[i] && x !== b.genes[i]) fromA++;
        if (x === b.genes[i] && x !== a.genes[i]) fromB++;
    });
    assert.ok(fromA > 0 && fromB > 0);
    assert.ok([a.blend, b.blend].includes(child.blend));
});

test("breed: same seed gives the same children, a new seed different ones", () => {
    const input = { parents: [CATALOG[0]!.config, CATALOG[2]!.config], count: 8, strength: 0.2, seed: 1234, bias: null, generation: 3 };
    const first = breed(input);
    assert.deepEqual(breed(input), first);
    assert.notDeepEqual(breed({ ...input, seed: 1235 }), first);
});

test("breed: count, ranges, names and parent indices", () => {
    const children = breed({ parents: [DEFAULT_EFFECT], count: 12, strength: 0.3, seed: 9, bias: null, generation: 2 });
    assert.equal(children.length, 12);
    children.forEach((c, i) => {
        assertWithinRanges(c.config);
        assert.equal(c.config.name, `untitled-g2-${i + 1}`);
        assert.deepEqual(c.parents, [0]);
    });
    // Names do not pile up generation suffixes.
    const again = breed({ parents: [children[0]!.config], count: 4, strength: 0.1, seed: 1, bias: null, generation: 3 });
    assert.equal(again[0]!.config.name, "untitled-g3-1");
});

test("breed: two or more parents cross two distinct parents per child", () => {
    const parents = [CATALOG[0]!.config, CATALOG[1]!.config, CATALOG[2]!.config];
    for (const c of breed({ parents, count: 16, strength: 0.1, seed: 3, bias: null, generation: 1 })) {
        assert.equal(c.parents.length, 2);
        assert.notEqual(c.parents[0], c.parents[1]);
    }
});

test("breed: the first parent's svg fields are inherited unchanged", () => {
    const svgParent = {
        ...DEFAULT_EFFECT,
        initial: { ...DEFAULT_EFFECT.initial, positionShape: "svgFill" as const, svgSource: "<svg/>", svgScale: 3 },
    };
    for (const c of breed({ parents: [svgParent], count: 6, strength: 0.5, seed: 11, bias: null, generation: 1 })) {
        assert.equal(c.config.initial.positionShape, "svgFill");
        assert.equal(c.config.initial.svgSource, "<svg/>");
        assert.equal(c.config.initial.svgScale, 3);
    }
});
