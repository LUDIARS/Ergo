// Cross-site / DNS-rebinding guard: the pure checks, then the wiring on a
// live server (HTTP middleware in createApp, WS handshake in attachWs).
// Hermetic: the only listener binds 127.0.0.1 on port 0.

import { test } from "node:test";
import assert from "node:assert/strict";
import { once } from "node:events";
import { request as httpRequest, type Server } from "node:http";
import type { AddressInfo } from "node:net";

import { serve } from "@hono/node-server";
import { WebSocket } from "ws";

import { crossSiteProblem, isTrustedHost, wsHandshakeProblem } from "../../src/core/request_guard.js";
import { attachWs, createApp, instantiatePlugins } from "../../src/core/server.js";
import makeLogPlugin from "../../src/plugins/log/index.js";
import { quietContext } from "../helpers/fake_ws.js";

test("isTrustedHost accepts loopback names, IP literals and the bind host only", () => {
    for (const h of ["localhost:5170", "app.localhost:5170", "127.0.0.1:5170", "[::1]:5170", "192.168.1.5:5170"]) {
        assert.equal(isTrustedHost(h), true, h);
    }
    for (const h of [undefined, "", "evil.example:5170", "localhost.evil.example:5170", "a b"]) {
        assert.equal(isTrustedHost(h), false, String(h));
    }
    assert.equal(isTrustedHost("devbox.lan:5170"), false);
    assert.equal(isTrustedHost("DevBox.lan:5170", "devbox.lan"), true);
});

test("crossSiteProblem allows no Origin and same-origin, refuses other sites", () => {
    assert.equal(crossSiteProblem("127.0.0.1:5170", undefined), null);
    assert.equal(crossSiteProblem("localhost:5170", "http://localhost:5170"), null);
    assert.equal(crossSiteProblem("LocalHost:5170", "http://localhost:5170"), null);
    assert.equal(crossSiteProblem("127.0.0.1:80", "http://127.0.0.1"), null);   // default port
    assert.notEqual(crossSiteProblem("127.0.0.1:5170", "https://evil.example"), null);
    assert.notEqual(crossSiteProblem("127.0.0.1:5170", "null"), null);
    assert.notEqual(crossSiteProblem("127.0.0.1:5170", "http://localhost:3000"), null);
    // DNS rebinding: Origin matches Host, but the Host is an attacker's name,
    // and a rebinding GET carries no Origin at all.
    assert.notEqual(crossSiteProblem("evil.example:5170", "http://evil.example:5170"), null);
    assert.notEqual(crossSiteProblem("evil.example:5170", undefined), null);
});

test("wsHandshakeProblem lets native clients (no Origin) dial any host name", () => {
    assert.equal(wsHandshakeProblem("devbox.lan:5170", undefined), null);
    assert.equal(wsHandshakeProblem("127.0.0.1:5170", "http://127.0.0.1:5170"), null);
    assert.notEqual(wsHandshakeProblem("127.0.0.1:5170", "https://evil.example"), null);
    assert.notEqual(wsHandshakeProblem("evil.example:5170", "http://evil.example:5170"), null);
});

/// Status of a raw HTTP request with the given headers (Host included).
function status(port: number, method: string, path: string, headers: Record<string, string>,
                body?: string): Promise<number> {
    return new Promise((resolve, reject) => {
        const req = httpRequest({ host: "127.0.0.1", port, method, path, headers }, (res) => {
            res.resume();
            resolve(res.statusCode ?? 0);
        });
        req.on("error", reject);
        req.end(body);
    });
}

test("the server refuses cross-site WS handshakes and HTTP requests", { timeout: 15_000 }, async () => {
    const logs: string[] = [];
    const ctx = quietContext(logs);
    const loaded = instantiatePlugins([makeLogPlugin], [], ctx.log);
    const { app, plugins } = createApp(loaded.plugins, loaded.rejected, ctx);
    const server = serve({ fetch: app.fetch, port: 0, hostname: "127.0.0.1" }) as Server;
    await once(server, "listening");
    const wss = attachWs(server, plugins, ctx);
    const port = (server.address() as AddressInfo).port;
    const handshake = (headers: Record<string, string>) => new Promise<string>((resolve) => {
        const ws = new WebSocket(`ws://127.0.0.1:${port}/log/ws`, { headers });
        ws.on("open",  () => { ws.terminate(); resolve("open"); });
        ws.on("error", (e) => resolve(e.message));
    });
    try {
        assert.equal(await handshake({}), "open");                                   // engine
        assert.equal(await handshake({ Host: `devbox.lan:${port}` }), "open");         // engine, LAN name
        assert.equal(await handshake({ Origin: `http://127.0.0.1:${port}` }), "open"); // own UI
        assert.match(await handshake({ Origin: "https://evil.example" }), /403/);
        assert.match(await handshake({ Host: `evil.example:${port}`, Origin: `http://evil.example:${port}` }), /403/);
        assert.ok(logs.some((l) => l.includes("refused WS /log/ws")), logs.join("\n"));

        const local = { Host: `127.0.0.1:${port}` };
        assert.equal(await status(port, "GET", "/log/api/apps", local), 200);
        assert.equal(await status(port, "GET", "/log/api/apps", { ...local, Origin: "https://evil.example" }), 403);
        assert.equal(await status(port, "GET", "/log/api/apps", { Host: `evil.example:${port}` }), 403);
        // A no-cors text/plain POST is a "simple" request: no preflight.
        assert.equal(await status(port, "POST", "/log/api/apps",
            { ...local, Origin: "https://evil.example", "Content-Type": "text/plain" }, "{}"), 403);
    } finally {
        for (const c of wss.clients) c.terminate();
        wss.close();
        server.closeAllConnections();
        await new Promise<void>((resolve) => server.close(() => resolve()));
    }
});
