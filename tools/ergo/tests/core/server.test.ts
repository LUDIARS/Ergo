// Shared server core: plugin instantiation and id rules, the aggregated
// /api endpoints, every built-in's mount points and WS upgrade routing.
// Hermetic: the only listener binds 127.0.0.1 on port 0.

import { test, before, after } from "node:test";
import assert from "node:assert/strict";
import { once } from "node:events";
import type { AddressInfo } from "node:net";
import type { Server } from "node:http";
import { fileURLToPath } from "node:url";

import { Hono } from "hono";
import { serve } from "@hono/node-server";
import { WebSocket } from "ws";

import type { Plugin, PluginFactory, PluginHealth } from "../../src/core/plugin.js";
import { PLUGIN_FACTORIES } from "../../src/core/registry.js";
import {
    PLUGIN_ID_PATTERN, attachWs, consoleContext, createApp, instantiatePlugins,
    type RejectedPlugin,
} from "../../src/core/server.js";
import makeParticlePlugin from "../../src/plugins/particle/index.js";
import { quietContext } from "../helpers/fake_ws.js";

const savedCwd = process.cwd();
const savedEnv = { backend: process.env.ERGO_VFX_BACKEND, model: process.env.ERGO_VFX_MODEL };

before(() => {
    // Static roots of the built-ins and the shell are cwd-relative.
    process.chdir(fileURLToPath(new URL("../../", import.meta.url)));
    // vfx-* pick their LLM backend from the environment; the default CLI
    // backend reports healthy without spawning anything.
    delete process.env.ERGO_VFX_BACKEND;
    delete process.env.ERGO_VFX_MODEL;
});

after(() => {
    process.chdir(savedCwd);
    if (savedEnv.backend !== undefined) process.env.ERGO_VFX_BACKEND = savedEnv.backend;
    if (savedEnv.model   !== undefined) process.env.ERGO_VFX_MODEL   = savedEnv.model;
});

function builtinApp() {
    const logs: string[] = [];
    const ctx = quietContext(logs);
    const loaded = instantiatePlugins(PLUGIN_FACTORIES, [], ctx.log);
    return { ...createApp(loaded.plugins, loaded.rejected, ctx), ctx, logs };
}

function plain(id: string, extra: Partial<Plugin> = {}): PluginFactory {
    return () => ({ id, title: id, ...extra });
}

test("consoleContext is the default PluginContext", () => {
    assert.equal(typeof consoleContext.log, "function");
});

test("/api/plugins lists every built-in once and / redirects to the shell", async () => {
    const { app, plugins, rejected, logs } = builtinApp();
    assert.equal(plugins.length, PLUGIN_FACTORIES.length);
    assert.deepEqual(rejected, []);
    assert.deepEqual(logs, []);

    const body = (await (await app.request("/api/plugins")).json()) as {
        plugins: { id: string; title: string; url: string; ws: string }[];
    };
    assert.equal(body.plugins.length, PLUGIN_FACTORIES.length);
    assert.equal(new Set(body.plugins.map((p) => p.id)).size, body.plugins.length, "ids are unique");
    for (const p of body.plugins) {
        assert.match(p.id, PLUGIN_ID_PATTERN);
        assert.equal(typeof p.title, "string");
        assert.equal(p.url, `/${p.id}/`);
        assert.equal(p.ws, `/${p.id}/ws`);
    }

    const root = await app.request("/");
    assert.equal(root.status, 302);
    assert.equal(root.headers.get("location"), "/shell/index.html");
    assert.equal((await app.request("/shell/index.html")).status, 200);
});

test("every built-in serves /<id>/ and /<id>/api/health, and /api/health aggregates them", async () => {
    const { app, plugins } = builtinApp();
    for (const p of plugins) {
        const ui = await app.request(`/${p.id}/`);
        assert.equal(ui.status, 200, `/${p.id}/`);
        assert.match(ui.headers.get("content-type") ?? "", /text\/html/, `/${p.id}/`);

        const health = await app.request(`/${p.id}/api/health`);
        assert.equal(health.status, 200, `/${p.id}/api/health`);
        assert.equal(typeof ((await health.json()) as { ok?: unknown }).ok, "boolean", p.id);
    }

    const res = await app.request("/api/health");
    assert.equal(res.status, 200);
    const body = (await res.json()) as { ok: boolean; plugins: PluginHealth[]; rejected: unknown[] };
    assert.equal(body.ok, true);
    assert.deepEqual(body.plugins.map((p) => p.id), plugins.map((p) => p.id));
    assert.deepEqual(body.rejected, []);
});

test("a throwing or malformed health() is reported as ok:false instead of a 500", async () => {
    const ctx = quietContext();
    const loaded = instantiatePlugins([
        makeParticlePlugin,
        plain("flaky", { health: () => { throw new Error("sensor offline"); } }),
        plain("vague", { health: () => ({ clients: 1 }) as unknown as PluginHealth }),
    ], [], ctx.log);
    const { app } = createApp(loaded.plugins, loaded.rejected, ctx);

    const res = await app.request("/api/health");
    assert.equal(res.status, 200);
    const body = (await res.json()) as { ok: boolean; plugins: (PluginHealth & { id: string })[] };
    const byId = Object.fromEntries(body.plugins.map((p) => [p.id, p]));
    assert.equal(body.ok, false);
    assert.equal(byId.particle.ok, true);
    assert.equal(byId.flaky.ok, false);
    assert.match(String(byId.flaky.err), /sensor offline/);
    assert.equal(byId.vague.ok, false);
});

test("built-in failures abort startup", () => {
    const log = quietContext().log;
    assert.throws(() => instantiatePlugins([makeParticlePlugin, makeParticlePlugin], [], log),
        /built-in plugin 'particle' rejected: duplicate id 'particle'/);
    assert.throws(() => instantiatePlugins([plain("api")], [], log), /reserved id 'api'/);
    assert.throws(() => instantiatePlugins([plain("a.b")], [], log), /invalid id 'a\.b'/);
    assert.throws(() => instantiatePlugins([() => { throw new Error("nope"); }], [], log), /factory threw: nope/);

    const loaded = instantiatePlugins([plain("broken", { routes: () => { throw new Error("no routes"); } })], [], log);
    assert.throws(() => createApp(loaded.plugins, loaded.rejected, quietContext()),
        /built-in plugin 'broken' rejected: routes\(\) threw: no routes/);
});

test("broken, reserved, invalid and duplicate external entries are skipped; the first id wins", async () => {
    const shadow: PluginFactory = () => ({
        id: "particle", title: "Shadow",
        routes() {
            const a = new Hono();
            a.get("/api/effect",      (c) => c.text("shadow"));
            a.get("/api/only-shadow", (c) => c.text("shadow"));
            return a;
        },
    });
    const ext = (source: string, factory: PluginFactory) => ({ source: `pack/${source}`, factory });
    const externals = [
        ext("dup",        shadow),
        ext("api",        plain("api")),
        ext("shell",      plain("shell")),
        ext("dots",       plain("a.b")),
        ext("upper",      plain("Upper")),
        ext("notitle",    () => ({ id: "notitle", title: 3 }) as unknown as Plugin),
        ext("badroutes",  () => ({ id: "badroutes", title: "x", routes: "x" }) as unknown as Plugin),
        ext("notobject",  () => null as unknown as Plugin),
        ext("throws",     () => { throw new Error("boom"); }),
        ext("routesthrow", plain("routesthrow", { routes: () => { throw new Error("no routes"); } })),
        ext("extra",      plain("extra")),
        ext("extra2",     plain("extra", { title: "second extra" })),
    ];

    const logs: string[] = [];
    const ctx = quietContext(logs);
    const loaded = instantiatePlugins([makeParticlePlugin], externals, ctx.log);
    const { app, plugins, rejected } = createApp(loaded.plugins, loaded.rejected, ctx);

    assert.deepEqual(plugins.map((p) => p.id), ["particle", "extra"]);
    const bySource = Object.fromEntries(rejected.map((r: RejectedPlugin) => [r.source, r]));
    assert.deepEqual(Object.keys(bySource).sort(), [
        "pack/api", "pack/badroutes", "pack/dots", "pack/dup", "pack/extra2", "pack/notitle",
        "pack/notobject", "pack/routesthrow", "pack/shell", "pack/throws", "pack/upper",
    ]);
    assert.match(bySource["pack/dup"].reason, /duplicate id 'particle' \(already provided by built-in\)/);
    assert.match(bySource["pack/extra2"].reason, /duplicate id 'extra' \(already provided by pack\/extra\)/);
    assert.match(bySource["pack/api"].reason, /reserved id 'api'/);
    assert.match(bySource["pack/shell"].reason, /reserved id 'shell'/);
    assert.match(bySource["pack/dots"].reason, /invalid id 'a\.b'/);
    assert.match(bySource["pack/upper"].reason, /invalid id 'Upper'/);
    assert.match(bySource["pack/notitle"].reason, /title is not a string/);
    assert.match(bySource["pack/badroutes"].reason, /routes is not a function/);
    assert.match(bySource["pack/notobject"].reason, /did not return an object/);
    assert.match(bySource["pack/throws"].reason, /factory threw: boom/);
    assert.match(bySource["pack/routesthrow"].reason, /routes\(\) threw: no routes/);
    assert.equal(bySource["pack/dup"].id, "particle");
    assert.equal(bySource["pack/throws"].id, "");
    assert.equal(logs.length, rejected.length);

    // The built-in keeps its routes; nothing of the shadow plugin is mounted.
    const effect = await app.request("/particle/api/effect");
    assert.equal(effect.status, 200);
    assert.ok("emission" in ((await effect.json()) as object));
    assert.equal((await app.request("/particle/api/only-shadow")).status, 404);

    const list = (await (await app.request("/api/plugins")).json()) as { plugins: { id: string }[] };
    assert.deepEqual(list.plugins.map((p) => p.id), ["particle", "extra"]);

    const health = (await (await app.request("/api/health")).json()) as {
        ok: boolean; plugins: { id: string; ok: boolean }[];
        rejected: { id: string; source: string; ok: boolean; err: string }[];
    };
    assert.equal(health.ok, false);
    assert.deepEqual(health.plugins.map((p) => [p.id, p.ok]), [["particle", true], ["extra", true]]);
    assert.equal(health.rejected.length, rejected.length);
    assert.ok(health.rejected.every((r) => r.ok === false && r.err.length > 0));
});

interface Frame { op: string; [key: string]: unknown }

/** Resolve with the first frame matching `pred`. */
function frame(ws: WebSocket, pred: (m: Frame) => boolean): Promise<Frame> {
    return new Promise((resolve, reject) => {
        const onMessage = (raw: Buffer) => {
            const m = JSON.parse(raw.toString()) as Frame;
            if (!pred(m)) return;
            ws.off("message", onMessage);
            resolve(m);
        };
        ws.on("message", onMessage);
        ws.once("error", reject);
    });
}

/** Resolve once the client is closed; report whether it ever opened. */
function refused(ws: WebSocket): Promise<boolean> {
    return new Promise((resolve) => {
        let opened = false;
        ws.on("open",  () => { opened = true; });
        ws.on("error", () => { /* expected: socket hang up */ });
        ws.on("close", () => resolve(!opened));
    });
}

test("attachWs routes upgrades by plugin id", { timeout: 15_000 }, async () => {
    const { app, plugins, ctx } = builtinApp();
    const server = serve({ fetch: app.fetch, port: 0, hostname: "127.0.0.1" }) as Server;
    await once(server, "listening");
    const wss = attachWs(server, plugins, ctx);
    const base = `ws://127.0.0.1:${(server.address() as AddressInfo).port}`;
    const clients: WebSocket[] = [];
    const open = (path: string) => {
        const ws = new WebSocket(`${base}${path}`);
        clients.push(ws);
        return ws;
    };

    try {
        const particle = open("/particle/ws");
        const state = await frame(particle, (m) => m.op === "state");
        assert.equal(typeof state.config, "object");

        const variable = open("/variable/ws");
        const registry = frame(variable, (m) => m.op === "registry");
        await once(variable, "open");
        variable.send(JSON.stringify({ op: "hello", role: "ui" }));
        assert.deepEqual((await registry).vars, []);

        // Unknown id, a plugin without onUpgrade, and a non-/ws path.
        for (const path of ["/nope/ws", "/vfx-breed/ws", "/particle/x", "/"]) {
            assert.equal(await refused(open(path)), true, path);
        }
    } finally {
        for (const c of clients) c.terminate();
        for (const c of wss.clients) c.terminate();
        wss.close();
        server.closeAllConnections();
        await new Promise<void>((resolve) => server.close(() => resolve()));
    }
});
