import { test } from "node:test";
import assert from "node:assert/strict";

import { parseScene } from "../../../src/plugins/scene_editor/parse_scene.js";

// Shape of ergo::scene::Scene::to_json() (src/scene/scene.cpp).
const ENGINE_SCENE = JSON.stringify({
    version: 1,
    id: "stage",
    domain: "level",
    mount: "/level/stage",
    imports: [],
    camera: { mode: "fly", target: [1, 2, 3], distance: 12, fov_deg: 60 },
    actors: [
        {
            id: "crate",
            name: "Crate",
            type: "PropActor",
            parent: null,
            transform: { pos: [1, 2, 3], rot: [0, 0.7071, 0, 0.7071], scale: [2, 2, 2] },
            visual: { kind: "mesh", ref: "assets/crate.mesh", material: "assets/crate.mat" },
            components: [{ type: "Collider", shape: "box", size: [1, 1, 1] }],
            vars: [
                { name: "health", type: "int", value: 100 },
                { name: "label", type: "string", value: "hi" },
                { name: "unset", type: "float" },
            ],
            instanceOf: "prefabs/crate.scene.json",
            overrides: [{ path: "vars/health", value: 50 }],
        },
        { id: "lamp", parent: "crate" },
    ],
});

test("parseScene maps an engine scene onto the UI snapshot", () => {
    const s = parseScene("stage", ENGINE_SCENE, "lookdev");
    assert.ok(s);
    assert.equal(s.id, "stage");
    assert.equal(s.app, "lookdev");
    assert.equal(s.mount, "/level/stage");
    assert.deepEqual(s.camera, { mode: "fly", target: [1, 2, 3], distance: 12, fov_deg: 60 });
    assert.equal(s.actors.length, 2);

    const crate = s.actors[0];
    assert.equal(crate.parent, "");                    // null -> root
    assert.deepEqual(crate.transform.rot, [0, 0.7071, 0, 0.7071]);
    assert.deepEqual(crate.visual, { kind: "mesh", ref: "assets/crate.mesh", material: "assets/crate.mat" });
    assert.equal(crate.instanceOf, "prefabs/crate.scene.json");
    assert.deepEqual(crate.overrides, [{ path: "vars/health", value: "50" }]);
    assert.deepEqual(crate.components, [
        { type: "Collider", props: [["shape", "\"box\""], ["size", "[1,1,1]"]] },
    ]);
});

test("var values are JSON text so the inspector can edit them as JSON", () => {
    const crate = parseScene("stage", ENGINE_SCENE, "app")!.actors[0];
    assert.deepEqual(crate.vars, [
        { name: "health", type: "int",    value: "100" },
        { name: "label",  type: "string", value: "\"hi\"" },
        { name: "unset",  type: "float",  value: "" },
    ]);
    assert.equal(JSON.parse(crate.vars[1].value), "hi");
});

test("missing fields fall back to ergo_scene defaults", () => {
    const lamp = parseScene("stage", ENGINE_SCENE, "app")!.actors[1];
    assert.equal(lamp.parent, "crate");
    assert.equal(lamp.type, "Actor");
    assert.equal(lamp.name, "");
    assert.deepEqual(lamp.transform, { pos: [0, 0, 0], rot: [0, 0, 0, 1], scale: [1, 1, 1] });
    assert.deepEqual(lamp.vars, []);
    assert.deepEqual(lamp.overrides, []);
    assert.deepEqual(lamp.components, []);
    assert.equal(lamp.instanceOf, "");
});

test("the frame id wins over the document id (patches route by it)", () => {
    const s = parseScene("relay-key", JSON.stringify({ id: "other", actors: [] }), "app");
    assert.equal(s?.id, "relay-key");
    const unnamed = parseScene("relay-key", JSON.stringify({ actors: [] }), "app");
    assert.equal(unnamed?.id, "relay-key");
    assert.equal(unnamed?.domain, "level");
    assert.equal(unnamed?.mount, "/level");
});

test("malformed or non-object documents are rejected", () => {
    assert.equal(parseScene("x", "{not json", "app"), null);
    assert.equal(parseScene("x", "[1,2]", "app"), null);
    assert.equal(parseScene("x", "null", "app"), null);
    // Non-object actor entries are skipped rather than crashing the relay.
    const s = parseScene("x", JSON.stringify({ actors: [1, null, "a", { id: "ok" }] }), "app");
    assert.deepEqual(s?.actors.map((a) => a.id), ["ok"]);
});
