// Pins the JSON contract between the variable plugin and ergo_bind
// (src/bind/bind_engine.cpp: hello / bind / value / unbind /
// actor_register / actor_unregister from the engine, set back to it).

import { test } from "node:test";
import assert from "node:assert/strict";

import type { Plugin } from "../../../src/core/plugin.js";
import makeVariablePlugin from "../../../src/plugins/variable/index.js";
import { connect, type FakeWs } from "../../helpers/fake_ws.js";

const SPEED = {
    op: "bind", name: "player.speed", kind: "float",
    meta: { min: 0, max: 10, step: 0.5, unit: "m/s", actor: 7 }, value: 3,
};

function engine(plugin: Plugin, app: string): FakeWs {
    const ws = connect(plugin);
    ws.receive({ op: "hello", role: "engine", app });
    return ws;
}

function ui(plugin: Plugin): FakeWs {
    const ws = connect(plugin);
    ws.receive({ op: "hello", role: "ui" });
    return ws;
}

test("a UI hello is primed with the actor tree and the registry", () => {
    const plugin = makeVariablePlugin();
    const eng = engine(plugin, "Game");
    eng.receive(SPEED);
    eng.receive({ op: "actor_register", handle: 7, parent: 0, name: "Player" });

    const u = ui(plugin);
    assert.deepEqual(u.sent.map((m) => m.op), ["actors", "registry"]);
    assert.deepEqual(u.last("actors")!.nodes, [{ handle: 7, parent: 0, name: "Player", app: "Game" }]);
    assert.deepEqual(u.last("registry")!.vars, [{
        name: "player.speed", kind: "float", meta: SPEED.meta, value: 3, app: "Game",
    }]);
    assert.deepEqual(eng.sent, [], "the engine never receives UI snapshots");
});

test("engine bind / value / unbind are relayed to every UI", () => {
    const plugin = makeVariablePlugin();
    const u1 = ui(plugin);
    const u2 = ui(plugin);
    const eng = engine(plugin, "Game");
    u1.clear(); u2.clear();

    eng.receive(SPEED);
    for (const u of [u1, u2]) {
        assert.equal((u.last("registry")!.vars as unknown[]).length, 1);
    }

    eng.receive({ op: "value", name: "player.speed", value: 4.5 });
    assert.deepEqual(u1.last("value"), { op: "value", name: "player.speed", value: 4.5 });
    assert.deepEqual(u2.last("value"), { op: "value", name: "player.speed", value: 4.5 });

    // A value for an unknown name is dropped, not relayed.
    u1.clear();
    eng.receive({ op: "value", name: "nope", value: 1 });
    assert.deepEqual(u1.sent, []);

    eng.receive({ op: "unbind", name: "player.speed" });
    assert.deepEqual(u1.last("registry")!.vars, []);
});

test("a UI set is forwarded only to the engine that owns the variable", () => {
    const plugin = makeVariablePlugin();
    const a = engine(plugin, "A");
    const b = engine(plugin, "B");
    a.receive(SPEED);
    b.receive({ ...SPEED, name: "enemy.hp", kind: "int32", meta: {}, value: 100 });

    const u = ui(plugin);
    u.receive({ op: "set", name: "player.speed", value: 9 });
    assert.deepEqual(a.sent, [{ op: "set", name: "player.speed", value: 9 }]);
    assert.deepEqual(b.sent, []);

    u.receive({ op: "set", name: "enemy.hp", value: [1, 2, 3] });
    assert.deepEqual(b.sent, [{ op: "set", name: "enemy.hp", value: [1, 2, 3] }]);

    // Unknown names are ignored.
    u.receive({ op: "set", name: "missing", value: 1 });
    assert.equal(a.sent.length + b.sent.length, 2);
});

test("an engine disconnect purges its variables and actors and rebroadcasts", () => {
    const plugin = makeVariablePlugin();
    const a = engine(plugin, "A");
    const b = engine(plugin, "B");
    a.receive(SPEED);
    a.receive({ op: "actor_register", handle: 7, parent: 0, name: "Player" });
    b.receive({ ...SPEED, name: "enemy.hp" });
    b.receive({ op: "actor_register", handle: 9, parent: 0, name: "Enemy" });

    const u = ui(plugin);
    u.clear();
    a.close();

    assert.deepEqual((u.last("registry")!.vars as { name: string }[]).map((v) => v.name), ["enemy.hp"]);
    assert.deepEqual((u.last("actors")!.nodes as { handle: number }[]).map((n) => n.handle), [9]);
    assert.deepEqual(plugin.health!(), { ok: true, vars: 1, actors: 1, clients: 2 });
});

test("actor_unregister is relayed; messages before hello and malformed frames are ignored", () => {
    const plugin = makeVariablePlugin();
    const u = ui(plugin);
    const eng = engine(plugin, "Game");
    eng.receive({ op: "actor_register", handle: 7, parent: 0, name: "Player" });
    eng.receive({ op: "actor_unregister", handle: 7 });
    assert.deepEqual(u.last("actors")!.nodes, []);

    // A socket that never said hello cannot bind or set.
    const anon = connect(plugin);
    anon.receive(SPEED);
    anon.receive({ op: "set", name: "player.speed", value: 1 });
    anon.emit("message", Buffer.from("not json"));
    assert.equal(plugin.health!().vars, 0);
    assert.deepEqual(anon.sent, []);
    assert.deepEqual(eng.sent, []);
});
