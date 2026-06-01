import { Hono } from "hono";
import { WebSocket as WS } from "ws";

import type { Plugin, PluginFactory } from "../../core/plugin.js";

interface BridgeConfig {
    patchUrl: string;
}

const factory: PluginFactory = () => {
    const cfg: BridgeConfig = { patchUrl: "http://127.0.0.1:8080/ui_layout/patch" };
    const clients = new Set<WS>();

    function broadcast(msg: unknown) {
        const raw = JSON.stringify(msg);
        for (const ws of clients) if (ws.readyState === WS.OPEN) ws.send(raw);
    }

    const plugin: Plugin = {
        id: "ui_layout",
        title: "UI Layout",
        icon: "🧩",
        description: "Data-driven UI layout editor with JSON patch live updates via custos bridge.",
        staticRoot: "./src/plugins/ui_layout/ui",
        routes() {
            const app = new Hono();
            app.get("/api/health", (c) => c.json({ ok: true, clients: clients.size, patchUrl: cfg.patchUrl }));
            app.get("/api/bridge/config", (c) => c.json(cfg));
            app.post("/api/bridge/config", async (c) => {
                const body = await c.req.json();
                if (typeof body.patchUrl === "string" && body.patchUrl.length > 0) cfg.patchUrl = body.patchUrl;
                return c.json({ ok: true, patchUrl: cfg.patchUrl });
            });
            app.post("/api/bridge/patch", async (c) => {
                const patch = await c.req.text();
                const res = await fetch(cfg.patchUrl, {
                    method: "POST",
                    headers: { "content-type": "application/json" },
                    body: patch,
                }).catch(() => null);
                broadcast({ op: "patch", payload: patch });
                return c.json({ ok: !!res && res.ok, status: res?.status ?? 0 });
            });
            return app;
        },
        onUpgrade(_req, ws) {
            clients.add(ws);
            ws.on("close", () => clients.delete(ws));
            ws.on("message", (raw) => {
                try {
                    const m = JSON.parse(raw.toString());
                    if (m?.op === "patch") broadcast(m);
                } catch {}
            });
        },
        health() {
            return { ok: true, clients: clients.size, patchUrl: cfg.patchUrl };
        },
    };

    return plugin;
};

export default factory;
