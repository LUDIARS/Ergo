import { test } from "node:test";
import assert from "node:assert/strict";
import type { IncomingMessage } from "node:http";
import type { WebSocket } from "ws";

import makeSceneEditorPlugin from "../../../src/plugins/scene_editor/index.js";
import type { Plugin, PluginContext } from "../../../src/core/plugin.js";

/// Just enough of a `ws` socket for the relay: an open readyState, `on`
/// handler registration and a recorded `send`.
class FakeSocket {
    readyState = 1;   // WebSocket.OPEN
    sent: Array<Record<string, unknown>> = [];
    private handlers = new Map<string, Array<(arg?: unknown) => void>>();

    on(event: string, cb: (arg?: unknown) => void) {
        const list = this.handlers.get(event) ?? [];
        list.push(cb);
        this.handlers.set(event, list);
        return this;
    }
    send(text: string) { this.sent.push(JSON.parse(text)); }

    emit(event: string, arg?: unknown) {
        for (const cb of this.handlers.get(event) ?? []) cb(arg);
    }
    /** Deliver a frame the way `ws` does (a Buffer). */
    deliver(msg: unknown) { this.emit("message", Buffer.from(JSON.stringify(msg))); }
    ops() { return this.sent.map((m) => m.op); }
}

const ctx: PluginContext = { log: () => {} };

function connect(plugin: Plugin): FakeSocket {
    const sock = new FakeSocket();
    plugin.onUpgrade!({} as IncomingMessage, sock as unknown as WebSocket, ctx);
    return sock;
}

function engine(plugin: Plugin, app: string): FakeSocket {
    const sock = connect(plugin);
    sock.deliver({ op: "hello", role: "engine", app });
    return sock;
}

function ui(plugin: Plugin): FakeSocket {
    const sock = connect(plugin);
    sock.deliver({ op: "hello", role: "ui" });
    return sock;
}

function sceneJson(id: string): string {
    return JSON.stringify({ id, actors: [{ id: "crate", name: "Crate", transform: { pos: [1, 2, 3] } }] });
}

test("an engine scene reaches connected UIs", () => {
    const plugin = makeSceneEditorPlugin();
    const view = ui(plugin);
    assert.deepEqual(view.sent, [{ op: "scenes", scenes: [] }]);

    const eng = engine(plugin, "lookdev");
    eng.deliver({ op: "scene", id: "stage", json: sceneJson("stage") });

    assert.deepEqual(view.ops(), ["scenes", "scene_update"]);
    const update = view.sent[1] as { scene: { id: string; app: string; actors: Array<{ id: string }> } };
    assert.equal(update.scene.id, "stage");
    assert.equal(update.scene.app, "lookdev");
    assert.deepEqual(update.scene.actors.map((a) => a.id), ["crate"]);
    assert.equal(eng.sent.length, 0);   // nothing echoed to the engine

    // A UI that connects later gets the stored scene in its snapshot.
    const late = ui(plugin);
    const snapshot = late.sent[0] as { op: string; scenes: Array<{ id: string }> };
    assert.equal(snapshot.op, "scenes");
    assert.deepEqual(snapshot.scenes.map((s) => s.id), ["stage"]);
});

test("a UI patch is forwarded only to the engine that owns the scene", () => {
    const plugin = makeSceneEditorPlugin();
    const a = engine(plugin, "a");
    const b = engine(plugin, "b");
    a.deliver({ op: "scene", id: "scene_a", json: sceneJson("scene_a") });
    b.deliver({ op: "scene", id: "scene_b", json: sceneJson("scene_b") });
    const view = ui(plugin);

    const patch = { op: "patch", scene_id: "scene_b", actor_id: "crate", field: "transform.rot", value: [0, 0, 0, 1] };
    view.deliver(patch);
    assert.deepEqual(b.sent, [patch]);
    assert.deepEqual(a.sent, []);

    // Unknown scene: dropped, nobody receives it.
    view.deliver({ ...patch, scene_id: "missing" });
    assert.equal(a.sent.length + b.sent.length, 1);
});

test("an engine close removes its scenes and tells the UIs", () => {
    const plugin = makeSceneEditorPlugin();
    const a = engine(plugin, "a");
    const b = engine(plugin, "b");
    a.deliver({ op: "scene", id: "s1", json: sceneJson("s1") });
    a.deliver({ op: "scene", id: "s2", json: sceneJson("s2") });
    b.deliver({ op: "scene", id: "s3", json: sceneJson("s3") });
    const view = ui(plugin);
    view.sent = [];

    a.emit("close");
    assert.deepEqual(view.sent, [
        { op: "scene_removed", id: "s1" },
        { op: "scene_removed", id: "s2" },
    ]);
    assert.equal(plugin.health!().scenes, 1);   // b's scene survives
});

test("scene_remove and frames before hello", () => {
    const plugin = makeSceneEditorPlugin();
    const view = ui(plugin);

    // Without hello the connection has no role: its frames are ignored.
    const anon = connect(plugin);
    anon.deliver({ op: "scene", id: "ghost", json: sceneJson("ghost") });
    assert.deepEqual(view.ops(), ["scenes"]);

    const eng = engine(plugin, "app");
    eng.deliver({ op: "scene", id: "s", json: "{broken" });     // unparsable: ignored
    eng.deliver({ op: "scene", id: "s", json: sceneJson("s") });
    eng.deliver({ op: "scene_remove", id: "s" });
    eng.deliver({ op: "scene_remove", id: "s" });               // already gone: no event
    assert.deepEqual(view.ops(), ["scenes", "scene_update", "scene_removed"]);
});
