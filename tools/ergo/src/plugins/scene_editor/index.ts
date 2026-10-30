/// scene_editor plugin — scene tree, actor inspector, WS patch relay.
///
/// Engine clients (in-repo: `ergo_scene_link`, include/ergo/scene/editor_link.h)
/// push full scene JSON via { op: "scene", id, json }.
/// UI clients receive structured SceneInfo snapshots and can apply
/// field-level patches via { op: "patch", scene_id, actor_id, field, value }.
/// Patches are forwarded to the engine that owns the scene; the engine applies
/// them with `Scene::apply_patch` and re-publishes the scene, so the UI only
/// ever shows engine state. Protocol: spec/feature/tool/scene_editor.md.

import { Hono } from "hono";
import { WebSocket as WS } from "ws";

import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";
import { parseScene } from "./parse_scene.js";
import type {
    EngineMsg, SceneInfo, ServerToEngine, ServerToUi, UiMsg,
} from "./protocol.js";

interface ConnState {
    ws:   WS;
    role: "engine" | "ui" | "unknown";
    app:  string;
}

interface StoredScene extends SceneInfo {
    ownerWs: WS;
}

const factory: PluginFactory = () => {
    const conns  = new Set<ConnState>();
    const scenes = new Map<string, StoredScene>();

    function send(ws: WS, msg: ServerToUi | ServerToEngine) {
        if (ws.readyState === WS.OPEN) ws.send(JSON.stringify(msg));
    }

    function broadcastToUis(msg: ServerToUi) {
        const text = JSON.stringify(msg);
        for (const c of conns) {
            if (c.role === "ui" && c.ws.readyState === WS.OPEN) c.ws.send(text);
        }
    }

    function sceneList(): SceneInfo[] {
        return [...scenes.values()].map(({ ownerWs: _w, ...s }) => s);
    }

    const plugin: Plugin = {
        id:          "scene_editor",
        title:       "Scene Editor",
        icon:        "🎬",
        description: "Scene tree · actor inspector · live WS patch relay for ergo_scene clients",
        staticRoot:  "./src/plugins/scene_editor/ui",

        routes(_ctx: PluginContext) {
            const app = new Hono();
            app.get("/api/health", (c) =>
                c.json({ ok: true, scenes: scenes.size, clients: conns.size })
            );
            app.get("/api/scenes", (c) => c.json(sceneList()));
            return app;
        },

        onUpgrade(_req, ws: WS, ctx: PluginContext) {
            const state: ConnState = { ws, role: "unknown", app: "" };
            conns.add(state);

            ws.on("message", (raw: unknown) => {
                let msg: EngineMsg | UiMsg;
                try { msg = JSON.parse(String(raw)); } catch { return; }
                if (!msg || typeof msg !== "object" || !("op" in msg)) return;

                if ((msg as { op: string }).op === "hello") {
                    const hello = msg as { op: "hello"; role?: string; app?: string };
                    if (hello.role === "engine") {
                        state.role = "engine";
                        state.app  = hello.app ?? "anonymous";
                        ctx.log("info", `[scene_editor] engine connected: ${state.app}`);
                    } else if (hello.role === "ui") {
                        state.role = "ui";
                        send(ws, { op: "scenes", scenes: sceneList() });
                    }
                    return;
                }

                // ---- Engine messages ----------------------------------------
                if (state.role === "engine") {
                    const em = msg as EngineMsg;
                    if (em.op === "scene") {
                        if (typeof em.id !== "string" || typeof em.json !== "string") return;
                        const parsed = parseScene(em.id, em.json, state.app);
                        if (parsed) {
                            scenes.set(em.id, { ...parsed, ownerWs: ws });
                            broadcastToUis({ op: "scene_update", scene: parsed });
                        }
                    } else if (em.op === "scene_remove") {
                        if (scenes.delete(em.id)) {
                            broadcastToUis({ op: "scene_removed", id: em.id });
                        }
                    }
                    return;
                }

                // ---- UI messages --------------------------------------------
                if (state.role === "ui") {
                    const um = msg as UiMsg;
                    if (um.op === "patch") {
                        const stored = scenes.get(um.scene_id);
                        if (!stored) return;
                        const fwd: ServerToEngine = {
                            op: "patch",
                            scene_id: um.scene_id,
                            actor_id: um.actor_id,
                            field:    um.field,
                            value:    um.value,
                        };
                        send(stored.ownerWs, fwd);
                    }
                }
            });

            ws.on("close", () => {
                conns.delete(state);
                if (state.role === "engine") {
                    // Remove scenes owned by this engine connection.
                    const removed: string[] = [];
                    for (const [id, s] of scenes) {
                        if (s.ownerWs === ws) { scenes.delete(id); removed.push(id); }
                    }
                    for (const id of removed) broadcastToUis({ op: "scene_removed", id });
                }
            });
            ws.on("error", () => { /* close handler tidies up */ });
        },

        health() {
            return { ok: true, scenes: scenes.size, clients: conns.size };
        },
    };

    return plugin;
};

export default factory;
