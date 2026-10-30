import { test } from "node:test";
import assert from "node:assert/strict";
import type { IncomingMessage } from "node:http";
import type { WebSocket } from "ws";

import makeLogPlugin, { MAX_APPS, MAX_ENGINES } from "../../../src/plugins/log/index.js";
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
    closedWith: number | null = null;
    close(code: number) {
        this.closedWith = code;
        this.readyState = 3;   // WebSocket.CLOSED
        this.emit("close");
    }

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

function line(msg: string, lvl = "INF", frame = 1) {
    return { lvl, frame, ts_us: frame * 16_000, tid: 7, msg };
}

async function getJson(plugin: Plugin, path: string): Promise<{ status: number; body: Record<string, unknown> }> {
    const res = await plugin.routes!(ctx).request(path);
    return { status: res.status, body: await res.json() as Record<string, unknown> };
}

test("engine lines reach connected UIs", () => {
    const plugin = makeLogPlugin();
    const view = ui(plugin);
    assert.deepEqual(view.sent, [{ op: "apps", apps: [] }]);

    const eng = engine(plugin, "game");
    assert.deepEqual(view.ops(), ["apps", "apps"]);   // engine announced
    assert.deepEqual(view.sent[1], {
        op: "apps", apps: [{ app: "game", connected: true, lines: 0, last_seq: 0, dropped: 0 }],
    });

    eng.deliver({ op: "lines", app: "game", dropped: 0, lines: [line("hi", "INF", 12), line("bad", "ERR", 13)] });
    assert.deepEqual(view.sent[2], {
        op: "lines", app: "game", dropped: 0,
        lines: [
            { ...line("hi", "INF", 12), seq: 1 },
            { ...line("bad", "ERR", 13), seq: 2 },
        ],
    });
    assert.equal(eng.sent.length, 0);   // nothing echoed to the engine
});

test("a UI that connects later gets the backlog", () => {
    const plugin = makeLogPlugin();
    const a = engine(plugin, "a");
    const b = engine(plugin, "b");
    a.deliver({ op: "lines", dropped: 3, lines: [line("a1"), line("a2")] });
    b.deliver({ op: "lines", dropped: 0, lines: [line("b1")] });

    const view = ui(plugin);
    assert.deepEqual(view.ops(), ["apps", "backlog", "backlog"]);
    const apps = (view.sent[0] as { apps: Array<{ app: string; lines: number; dropped: number }> }).apps;
    assert.deepEqual(apps.map((x) => [x.app, x.lines, x.dropped]), [["a", 2, 3], ["b", 1, 0]]);
    const backlogA = view.sent[1] as { app: string; dropped: number; lines: Array<{ msg: string; seq: number }> };
    assert.equal(backlogA.app, "a");
    assert.equal(backlogA.dropped, 3);
    assert.deepEqual(backlogA.lines.map((l) => [l.msg, l.seq]), [["a1", 1], ["a2", 2]]);
});

test("engines sharing a name are de-duplicated; a restarted engine continues its ring", () => {
    const plugin = makeLogPlugin();
    const first  = engine(plugin, "game");
    const second = engine(plugin, "game");
    const third  = engine(plugin, "game");
    first.deliver({ op: "lines", lines: [line("from first")] });
    second.deliver({ op: "lines", lines: [line("from second")] });
    third.deliver({ op: "lines", lines: [line("from third")] });

    const view = ui(plugin);
    const names = (view.sent[0] as { apps: Array<{ app: string }> }).apps.map((x) => x.app);
    assert.deepEqual(names, ["game", "game#2", "game#3"]);

    // `game` disconnects: its lines stay; the next `game` engine takes the ring over.
    first.emit("close");
    const appsMsg = view.sent.at(-1) as { op: string; apps: Array<{ app: string; connected: boolean }> };
    assert.equal(appsMsg.op, "apps");
    assert.deepEqual(appsMsg.apps.map((x) => [x.app, x.connected]),
        [["game", false], ["game#2", true], ["game#3", true]]);

    const restarted = engine(plugin, "game");
    restarted.deliver({ op: "lines", lines: [line("after restart")] });
    const live = view.sent.at(-1) as { op: string; app: string; lines: Array<{ msg: string; seq: number }> };
    assert.equal(live.op, "lines");
    assert.equal(live.app, "game");
    assert.deepEqual(live.lines.map((l) => [l.msg, l.seq]), [["after restart", 2]]);
});

test("lines before hello register the engine by the frame's app; hello after is ignored", () => {
    const plugin = makeLogPlugin();
    const view = ui(plugin);
    const eng = connect(plugin);
    eng.deliver({ op: "lines", app: "early", dropped: 0, lines: [line("first")] });
    eng.deliver({ op: "hello", role: "engine", app: "early" });
    eng.deliver({ op: "hello", role: "engine", app: "renamed" });

    assert.deepEqual(view.ops(), ["apps", "apps", "lines"]);
    const apps = (view.sent[1] as { apps: Array<{ app: string }> }).apps;
    assert.deepEqual(apps.map((x) => x.app), ["early"]);
    assert.equal(plugin.health!().apps, 1);
});

test("malformed frames and UI-sent lines are ignored", () => {
    const plugin = makeLogPlugin();
    const view = ui(plugin);
    const eng = engine(plugin, "game");
    view.sent = [];

    eng.emit("message", Buffer.from("{not json"));
    eng.deliver({ op: "lines", lines: "nope" });
    eng.deliver({ op: "lines", lines: [{ lvl: "LOUD", frame: 1, ts_us: 0, tid: 0, msg: "x" }] });
    assert.deepEqual(view.sent, []);   // no valid line, no drop count: nothing to broadcast

    view.deliver({ op: "lines", app: "game", lines: [line("spoofed")] });
    assert.deepEqual(view.sent, []);
    assert.equal(plugin.health!().apps, 1);
});

test("disconnected apps beyond MAX_APPS are evicted, live ones never", () => {
    const plugin = makeLogPlugin();
    const socks = [];
    for (let i = 0; i < MAX_APPS; ++i) socks.push(engine(plugin, `app${i}`));
    socks[3].emit("close");
    socks[5].emit("close");
    engine(plugin, "late");   // one over the limit: the oldest disconnected app goes

    const names = (ui(plugin).sent[0] as { apps: Array<{ app: string }> }).apps.map((x) => x.app);
    assert.equal(names.length, MAX_APPS);
    assert.equal(names.includes("app3"), false);
    assert.equal(names.includes("app5"), true);
    assert.equal(names.includes("late"), true);
});

test("engines past MAX_ENGINES are refused with 1013, so the rings stay bounded", () => {
    const plugin = makeLogPlugin();
    for (let i = 0; i < MAX_ENGINES; ++i) assert.equal(engine(plugin, `app${i}`).closedWith, null);
    const extra = engine(plugin, "one-too-many");
    assert.equal(extra.closedWith, 1013);
    // A refused socket's lines (which could name another app) are ignored.
    const late = connect(plugin);
    late.deliver({ op: "lines", app: "sneaky", dropped: 0, lines: [line("x")] });
    assert.equal(late.closedWith, 1013);
    const h = plugin.health!();
    assert.equal(h.engines, MAX_ENGINES);
    assert.equal(h.apps, MAX_APPS);
});

test("GET /api/lines filters by level, since, text and frame", async () => {
    const plugin = makeLogPlugin();
    const eng = engine(plugin, "game");
    eng.deliver({ op: "lines", dropped: 1, lines: [
        line("loading", "INF", 1),
        line("slow frame", "WRN", 2),
        line("Frame dropped", "ERR", 3),
        line("tick", "DBG", 4),
    ] });

    const all = await getJson(plugin, "/api/lines?app=game");
    assert.equal(all.status, 200);
    assert.equal(all.body.last_seq, 4);
    assert.equal(all.body.dropped, 1);
    assert.equal((all.body.lines as unknown[]).length, 4);

    const msgs = async (q: string) =>
        ((await getJson(plugin, `/api/lines?app=game&${q}`)).body.lines as Array<{ msg: string }>).map((l) => l.msg);
    assert.deepEqual(await msgs("level=ERR,wrn"), ["slow frame", "Frame dropped"]);
    assert.deepEqual(await msgs("since=2"), ["Frame dropped", "tick"]);
    assert.deepEqual(await msgs("q=FRAME"), ["slow frame", "Frame dropped"]);
    assert.deepEqual(await msgs("frame_min=2&frame_max=3"), ["slow frame", "Frame dropped"]);
    assert.deepEqual(await msgs("limit=1"), ["tick"]);

    const apps = await getJson(plugin, "/api/apps");
    assert.deepEqual(apps.body, { apps: [{ app: "game", connected: true, lines: 4, last_seq: 4, dropped: 1 }] });
});

test("GET /api/lines rejects bad requests", async () => {
    const plugin = makeLogPlugin();
    engine(plugin, "game");
    assert.equal((await getJson(plugin, "/api/lines")).status, 400);
    assert.equal((await getJson(plugin, "/api/lines?app=nope")).status, 404);
    assert.equal((await getJson(plugin, "/api/lines?app=game&level=LOUD")).status, 400);
    assert.equal((await getJson(plugin, "/api/lines?app=game&since=abc")).status, 400);
    assert.equal((await getJson(plugin, "/api/lines?app=game&limit=0")).status, 400);
});
