/// Particle plugin — hosts a WS hub that mirrors a single
/// `ParticleEffectConfig` to all connected clients. Browser UIs send
/// `set` / `replace`; engine clients (e.g. AdventureCube via
/// `ergo_particle`) just receive `state` frames.
///
/// Migrated from the stand-alone `tools/particle-editor/` package.

import { Hono } from "hono";
import { WebSocket as WS } from "ws";

import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";
import {
    DEFAULT_EFFECT,
    mergeConfig,
    SCHEMA_VERSION,
    type Inbound,
    type Outbound,
    type ParticleEffectConfig,
} from "./schema.js";

const factory: PluginFactory = () => {
    let current: ParticleEffectConfig = JSON.parse(JSON.stringify(DEFAULT_EFFECT));
    const clients = new Set<WS>();

    function send(ws: WS, msg: Outbound) {
        if (ws.readyState === WS.OPEN) ws.send(JSON.stringify(msg));
    }

    function broadcastState() {
        const msg: Outbound = { op: "state", config: current, clients: clients.size };
        const text = JSON.stringify(msg);
        for (const c of clients) {
            if (c.readyState === WS.OPEN) c.send(text);
        }
    }

    const plugin: Plugin = {
        id:         "particle",
        title:      "Particle Editor",
        icon:       "✨",
        description:
            "Edit a ParticleEffectConfig live; broadcasts to ergo_particle clients over WS.",
        staticRoot: "./src/plugins/particle/ui",

        routes(_ctx: PluginContext) {
            const app = new Hono();

            app.get("/api/effect", (c) => c.json(current));
            app.post("/api/effect", async (c) => {
                const body = await c.req.json().catch(() => null);
                if (!body || typeof body !== "object") {
                    return c.json({ ok: false, err: "json" }, 400);
                }
                current = mergeConfig(current, body);
                broadcastState();
                return c.json({ ok: true, config: current });
            });

            app.get("/api/health", (c) =>
                c.json({ ok: true, version: SCHEMA_VERSION, clients: clients.size })
            );
            return app;
        },

        onUpgrade(_req, ws: WS, _ctx) {
            clients.add(ws);
            // Hand the new client the current state immediately.
            send(ws, { op: "state", config: current, clients: clients.size });
            broadcastState();

            ws.on("message", (raw: any) => {
                let msg: Inbound;
                try {
                    msg = JSON.parse(raw.toString());
                } catch {
                    return;
                }
                if (!msg || typeof msg !== "object" || !("op" in msg)) return;

                switch (msg.op) {
                    case "set":
                        current = mergeConfig(current, msg.config ?? {});
                        broadcastState();
                        break;
                    case "replace":
                        if (msg.config && typeof msg.config === "object") {
                            current = mergeConfig(DEFAULT_EFFECT, msg.config);
                            broadcastState();
                        }
                        break;
                    case "ping":
                        send(ws, { op: "ack" });
                        break;
                }
            });

            ws.on("close", () => {
                clients.delete(ws);
                broadcastState();
            });
            ws.on("error", () => { /* close handler tidies up */ });
        },

        health() {
            return { ok: true, version: SCHEMA_VERSION, clients: clients.size };
        },
    };

    return plugin;
};

export default factory;
