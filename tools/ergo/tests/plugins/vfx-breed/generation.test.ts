import { test } from "node:test";
import assert from "node:assert/strict";

import { DEFAULT_EFFECT } from "../../../src/plugins/particle/schema.js";
import { GENES, geneIndex } from "../../../src/plugins/vfx-breed/genes.js";
import { parseWeights, toBias } from "../../../src/plugins/vfx-breed/direction.js";
import { makeAdvisor, unavailableAdvisor, type DirectionAdvisor } from "../../../src/plugins/vfx-breed/advisor.js";
import { parseGenerationRequest, COUNT_MAX, COUNT_MIN } from "../../../src/plugins/vfx-breed/request.js";
import { runGeneration } from "../../../src/plugins/vfx-breed/generation.js";
import { createVfxBreedPlugin } from "../../../src/plugins/vfx-breed/index.js";

function fakeAdvisor(weights: Record<string, number>): DirectionAdvisor & { calls: number } {
    const a = {
        calls: 0,
        health: () => ({ ok: true, backend: "fake", model: "fake" }),
        suggest: async () => { a.calls++; return weights; },
    };
    return a;
}

const throwingAdvisor: DirectionAdvisor = {
    health: () => ({ ok: true, backend: "fake", model: "fake" }),
    suggest: async () => { throw new Error("boom"); },
};

const req = (body: object) => {
    const r = parseGenerationRequest(body, () => 77);
    assert.ok(r.ok);
    return r.req;
};

test("parseWeights accepts fenced / bare JSON and drops unknown genes", () => {
    assert.deepEqual(
        parseWeights('Sure:\n```json\n{"weights":{"initial.lifetimeMax":-0.8,"nope":1,"initial.size":"x"}}\n```'),
        { "initial.lifetimeMax": -0.8 },
    );
    assert.deepEqual(parseWeights('{"initial.size": 0.5}'), { "initial.size": 0.5 });
});

test("toBias aligns with GENES and clamps to -1..1", () => {
    const bias = toBias({ "initial.size": 3, "forces.gravity.y": -0.4 });
    assert.equal(bias.length, GENES.length);
    assert.equal(bias[geneIndex("initial.size")], 1);
    assert.equal(bias[geneIndex("forces.gravity.y")], -0.4);
    assert.equal(bias.filter((x) => x !== 0).length, 2);
});

test("request: defaults, clamps and selection", () => {
    const r = req({});
    assert.deepEqual(r.parents, [DEFAULT_EFFECT]);
    assert.equal(r.count, 8);
    assert.equal(r.seed, 77);
    assert.equal(req({ count: 100 }).count, COUNT_MAX);
    assert.equal(req({ count: 1 }).count, COUNT_MIN);
    assert.equal(req({ strength: 5 }).strength, 1);
    const a = { name: "a" }, b = { name: "b" }, c = { name: "c" };
    assert.deepEqual(req({ parents: [a, b, c], selected: [2, 0, 2] }).parents, [c, a]);
    assert.equal(parseGenerationRequest({ parents: [a], selected: [3] }).ok, false);
    assert.equal(parseGenerationRequest({ parents: "x" }).ok, false);
    assert.equal(parseGenerationRequest(null).ok, false);
});

test("no direction: the advisor is not asked", async () => {
    const adv = fakeAdvisor({ "initial.size": 1 });
    const res = await runGeneration(req({ seed: 5 }), adv);
    assert.equal(adv.calls, 0);
    assert.equal(res.direction.status, "none");
    assert.equal(res.children.length, 8);
    assert.equal(res.generation, 1);
});

test("direction weights shift the children (same seed, with vs without)", async () => {
    const body = { seed: 21, count: 16, strength: 0.15, direction: "もっと大きく" };
    const plain = await runGeneration(req({ ...body, direction: "" }), fakeAdvisor({}));
    const steered = await runGeneration(req(body), fakeAdvisor({ "initial.size": 1 }));
    assert.equal(steered.direction.status, "applied");
    const avg = (r: typeof plain) => r.children.reduce((s, c) => s + c.config.initial.size, 0) / r.children.length;
    assert.ok(avg(steered) > avg(plain), `${avg(steered)} > ${avg(plain)}`);
});

test("LLM unavailable: breeding still works and the status says why", async () => {
    const res = await runGeneration(req({ direction: "鋭く", seed: 3 }), unavailableAdvisor("cli", "no claude"));
    assert.equal(res.direction.status, "unavailable");
    assert.equal(res.direction.reason, "no claude");
    assert.equal(res.children.length, 8);
    // Same seed without a direction gives the same children: no bias leaked in.
    const plain = await runGeneration(req({ seed: 3 }), unavailableAdvisor("cli", "no claude"));
    assert.deepEqual(res.children, plain.children);
});

test("LLM failure: breeding still works", async () => {
    const res = await runGeneration(req({ direction: "鋭く" }), throwingAdvisor);
    assert.equal(res.direction.status, "failed");
    assert.match(res.direction.reason ?? "", /boom/);
    assert.equal(res.children.length, 8);
});

test("makeAdvisor never throws on a bad backend", () => {
    const a = makeAdvisor({ ERGO_VFX_BACKEND: "nope" });
    assert.equal(a.health().ok, false);
    const sdk = makeAdvisor({ ERGO_VFX_BACKEND: "sdk" });
    assert.equal(sdk.health().ok, false);
});

test("routes: POST /api/generation and GET /api/seeds", async () => {
    const app = createVfxBreedPlugin(unavailableAdvisor("cli", "off")).routes!({ log: () => {} });
    const res = await app.request("/api/generation", {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ parents: [DEFAULT_EFFECT], count: 4, seed: 8, direction: "x" }),
    });
    assert.equal(res.status, 200);
    const body = await res.json() as { ok: boolean; children: unknown[]; seed: number; direction: { status: string } };
    assert.equal(body.ok, true);
    assert.equal(body.children.length, 4);
    assert.equal(body.seed, 8);
    assert.equal(body.direction.status, "unavailable");

    const bad = await app.request("/api/generation", { method: "POST", body: "not json" });
    assert.equal(bad.status, 400);

    const seeds = await (await app.request("/api/seeds")).json() as { id: string }[];
    assert.equal(seeds[0]!.id, "default");
    assert.ok(seeds.length > 1);
});
