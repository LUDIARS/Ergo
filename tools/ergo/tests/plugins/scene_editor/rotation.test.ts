import { test } from "node:test";
import assert from "node:assert/strict";
// Browser modules are shipped as plain JavaScript, without declaration files.
// @ts-ignore
import { eulerDegToQuat, quatToEulerDeg } from "../../../src/plugins/scene_editor/ui/rotation.js";

function near(actual: number[], expected: number[], eps = 1e-6) {
    assert.equal(actual.length, expected.length);
    actual.forEach((v, i) => assert.ok(Math.abs(v - expected[i]) < eps, `[${i}] ${v} vs ${expected[i]}`));
}

const H = Math.SQRT1_2;

test("single-axis rotations map to the expected quaternions", () => {
    near(eulerDegToQuat([0, 0, 0]),  [0, 0, 0, 1]);
    near(eulerDegToQuat([90, 0, 0]), [H, 0, 0, H]);
    near(eulerDegToQuat([0, 90, 0]), [0, H, 0, H]);
    near(eulerDegToQuat([0, 0, 90]), [0, 0, H, H]);
});

test("the inspector round-trips Euler degrees through the wire quaternion", () => {
    for (const e of [[10, 20, 30], [-45, 15, 170], [0, -60, -90], [120, 5, 0]]) {
        const q = eulerDegToQuat(e);
        assert.ok(Math.abs(Math.hypot(...q) - 1) < 1e-9);   // unit length on the wire
        near(quatToEulerDeg(q), e, 1e-6);
    }
});

test("non-unit quaternions from the engine are normalized for display", () => {
    near(quatToEulerDeg([0, 2, 0, 2]), [0, 90, 0], 1e-4);
    near(quatToEulerDeg([0, 0, 0, 0]), [0, 0, 0]);   // degenerate input does not produce NaN
});
