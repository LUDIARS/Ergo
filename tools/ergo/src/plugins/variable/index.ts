/// Variable plugin — registry of variables bound by `ergo_bind` engine
/// clients, forwarded to / from browser UI clients over a single WS
/// hub.
///
/// Migrated from the stand-alone `tools/variable-editor/` package.

import { Hono } from "hono";
import { WebSocket as WS } from "ws";

import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";
import type {
    BoundVar, EngineMsg, ServerToEngine, ServerToUi, UiMsg, VarValue,
} from "./protocol.js";

interface ConnState {
    ws:   WS;
    role: "engine" | "ui" | "unknown";
    app:  string;
}

const factory: PluginFactory = () => {
    const conns    = new Set<ConnState>();
    const registry = new Map<string, BoundVar & { ownerWs: WS }>();

    function listVars(): BoundVar[] {
        return [...registry.values()].map((v) => ({
            name: v.name, kind: v.kind, meta: v.meta, value: v.value, app: v.app,
        }));
    }

    function send(ws: WS, msg: ServerToUi | ServerToEngine) {
        if (ws.readyState === WS.OPEN) ws.send(JSON.stringify(msg));
    }

    function broadcastRegistry() {
        const msg: ServerToUi = { op: "registry", vars: listVars() };
        const text = JSON.stringify(msg);
        for (const c of conns) {
            if (c.role === "ui" && c.ws.readyState === WS.OPEN) c.ws.send(text);
        }
    }

    function broadcastValue(name: string, value: VarValue) {
        const msg: ServerToUi = { op: "value", name, value };
        const text = JSON.stringify(msg);
        for (const c of conns) {
            if (c.role === "ui" && c.ws.readyState === WS.OPEN) c.ws.send(text);
        }
    }

    const plugin: Plugin = {
        id:         "variable",
        title:      "Variable Editor",
        icon:       "🎛",
        description:
            "Live-edit variables exposed by engine clients via BIND_VAR() (ergo_bind).",
        staticRoot: "./src/plugins/variable/ui",

        routes(_ctx: PluginContext) {
            const app = new Hono();

            app.get("/api/health", (c) =>
                c.json({ ok: true, vars: registry.size, clients: conns.size })
            );
            app.get("/api/vars", (c) => c.json(listVars()));
            return app;
        },

        onUpgrade(_req, ws: WS, _ctx) {
            const state: ConnState = { ws, role: "unknown", app: "" };
            conns.add(state);

            ws.on("message", (raw: any) => {
                let msg: EngineMsg | UiMsg;
                try { msg = JSON.parse(raw.toString()); } catch { return; }
                if (!msg || typeof msg !== "object" || !("op" in msg)) return;

                if (msg.op === "hello") {
                    if ("role" in msg && msg.role === "engine") {
                        state.role = "engine";
                        state.app  = (msg as any).app ?? "anonymous";
                    } else if ("role" in msg && msg.role === "ui") {
                        state.role = "ui";
                        send(ws, { op: "registry", vars: listVars() });
                    }
                    return;
                }

                // Engine messages
                if (state.role === "engine") {
                    const em = msg as EngineMsg;
                    if (em.op === "bind") {
                        registry.set(em.name, {
                            name: em.name, kind: em.kind, meta: em.meta, value: em.value,
                            app: state.app, ownerWs: ws,
                        });
                        broadcastRegistry();
                    } else if (em.op === "value") {
                        const v = registry.get(em.name);
                        if (v) {
                            v.value = em.value;
                            broadcastValue(em.name, em.value);
                        }
                    } else if (em.op === "unbind") {
                        if (registry.delete(em.name)) broadcastRegistry();
                    }
                    return;
                }

                // UI messages
                if (state.role === "ui") {
                    const um = msg as UiMsg;
                    if (um.op === "set") {
                        const v = registry.get(um.name);
                        if (!v) return;
                        const fwd: ServerToEngine = { op: "set", name: um.name, value: um.value };
                        if (v.ownerWs.readyState === WS.OPEN) v.ownerWs.send(JSON.stringify(fwd));
                    }
                }
            });

            ws.on("close", () => {
                conns.delete(state);
                if (state.role === "engine") {
                    let dirty = false;
                    for (const [name, v] of registry) {
                        if (v.ownerWs === ws) { registry.delete(name); dirty = true; }
                    }
                    if (dirty) broadcastRegistry();
                }
            });
            ws.on("error", () => { /* close handler tidies up */ });
        },

        health() {
            return { ok: true, vars: registry.size, clients: conns.size };
        },
    };

    return plugin;
};

export default factory;
