/// Variable plugin — registry of variables bound by `ergo_bind` engine
/// clients, forwarded to / from browser UI clients over a single WS hub.
///
/// v2: tracks an Actor tree alongside the variable registry so the UI
/// can group vars by owning actor (see `ergo_actor`).

import { Hono } from "hono";
import { WebSocket as WS } from "ws";

import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";
import type {
    ActorNode, BoundVar, EngineMsg, ServerToEngine, ServerToUi, UiMsg, VarValue,
} from "./protocol.js";

interface ConnState {
    ws:   WS;
    role: "engine" | "ui" | "unknown";
    app:  string;
}

const factory: PluginFactory = () => {
    const conns    = new Set<ConnState>();
    const registry = new Map<string, BoundVar & { ownerWs: WS }>();

    // handle -> ActorNode (+ owner ws so we can purge on disconnect).
    interface InternalActor extends ActorNode { ownerWs: WS; }
    const actors = new Map<number, InternalActor>();

    function listVars(): BoundVar[] {
        return [...registry.values()].map((v) => ({
            name: v.name, kind: v.kind, meta: v.meta, value: v.value, app: v.app,
        }));
    }

    function listActors(): ActorNode[] {
        return [...actors.values()].map((a) => ({
            handle: a.handle, parent: a.parent, name: a.name, app: a.app,
        }));
    }

    function send(ws: WS, msg: ServerToUi | ServerToEngine) {
        if (ws.readyState === WS.OPEN) ws.send(JSON.stringify(msg));
    }

    function broadcastToUis(msg: ServerToUi) {
        const text = JSON.stringify(msg);
        for (const c of conns) {
            if (c.role === "ui" && c.ws.readyState === WS.OPEN) c.ws.send(text);
        }
    }

    function broadcastRegistry() { broadcastToUis({ op: "registry", vars:  listVars()   }); }
    function broadcastActors()   { broadcastToUis({ op: "actors",   nodes: listActors() }); }

    function broadcastValue(name: string, value: VarValue) {
        broadcastToUis({ op: "value", name, value });
    }

    const plugin: Plugin = {
        id:         "variable",
        title:      "Variable Editor",
        icon:       "🎛",
        description:
            "Live-edit variables exposed by engine clients via BIND_VAR() (ergo_bind), grouped by ergo_actor.",
        staticRoot: "./src/plugins/variable/ui",

        routes(_ctx: PluginContext) {
            const app = new Hono();

            app.get("/api/health", (c) =>
                c.json({ ok: true, vars: registry.size, actors: actors.size, clients: conns.size })
            );
            app.get("/api/vars",   (c) => c.json(listVars()));
            app.get("/api/actors", (c) => c.json(listActors()));
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
                        // Prime the UI with current snapshots.
                        send(ws, { op: "actors",   nodes: listActors() });
                        send(ws, { op: "registry", vars:  listVars()   });
                    }
                    return;
                }

                // ---- Engine messages ------------------------------------
                if (state.role === "engine") {
                    const em = msg as EngineMsg;
                    switch (em.op) {
                        case "bind":
                            registry.set(em.name, {
                                name: em.name, kind: em.kind, meta: em.meta, value: em.value,
                                app: state.app, ownerWs: ws,
                            });
                            broadcastRegistry();
                            break;

                        case "value": {
                            const v = registry.get(em.name);
                            if (v) {
                                v.value = em.value;
                                broadcastValue(em.name, em.value);
                            }
                            break;
                        }

                        case "unbind":
                            if (registry.delete(em.name)) broadcastRegistry();
                            break;

                        case "actor_register":
                            actors.set(em.handle, {
                                handle: em.handle, parent: em.parent, name: em.name,
                                app: state.app, ownerWs: ws,
                            });
                            broadcastActors();
                            break;

                        case "actor_unregister":
                            if (actors.delete(em.handle)) broadcastActors();
                            break;
                    }
                    return;
                }

                // ---- UI messages ----------------------------------------
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
                    // Drop this engine's vars + actors.
                    let vdirty = false, adirty = false;
                    for (const [name, v] of registry) {
                        if (v.ownerWs === ws) { registry.delete(name); vdirty = true; }
                    }
                    for (const [h, a] of actors) {
                        if (a.ownerWs === ws) { actors.delete(h); adirty = true; }
                    }
                    if (adirty) broadcastActors();
                    if (vdirty) broadcastRegistry();
                }
            });
            ws.on("error", () => { /* close handler tidies up */ });
        },

        health() {
            return { ok: true, vars: registry.size, actors: actors.size, clients: conns.size };
        },
    };

    return plugin;
};

export default factory;
