/// scene_editor plugin — scene tree, actor inspector, WS patch relay.
///
/// Engine clients push full scene JSON via { op: "scene", id, json }.
/// UI clients receive structured SceneInfo snapshots and can apply
/// field-level patches via { op: "patch", scene_id, actor_id, field, value }.
/// Patches are forwarded to the engine that owns the scene.

import { Hono } from "hono";
import { WebSocket as WS } from "ws";

import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";
import type {
    ActorSummary, EngineMsg, SceneInfo, ServerToEngine, ServerToUi, UiMsg,
} from "./protocol.js";

interface ConnState {
    ws:   WS;
    role: "engine" | "ui" | "unknown";
    app:  string;
}

interface StoredScene extends SceneInfo {
    ownerWs: WS;
}

function parseScene(id: string, json: string, app: string): SceneInfo | null {
    let root: Record<string, unknown>;
    try { root = JSON.parse(json); } catch { return null; }

    const rawActors = Array.isArray(root.actors) ? root.actors as unknown[] : [];
    const actors: ActorSummary[] = rawActors
        .filter((a): a is Record<string, unknown> => typeof a === "object" && a !== null)
        .map((a) => {
            const tr = (a.transform as Record<string, unknown>) ?? {};
            const vi = (a.visual as Record<string, unknown>) ?? {};
            const pos   = asVec3(tr.pos)   ?? [0, 0, 0] as [number, number, number];
            const rot   = asVec4(tr.rot)   ?? [0, 0, 0, 1] as [number, number, number, number];
            const scale = asVec3(tr.scale) ?? [1, 1, 1] as [number, number, number];
            return {
                id:        String(a.id ?? ""),
                name:      String(a.name ?? ""),
                type:      String(a.type ?? "Actor"),
                parent:    typeof a.parent === "string" ? a.parent : "",
                transform: { pos, rot, scale },
                visual: {
                    kind:     String(vi.kind ?? ""),
                    ref:      String(vi.ref ?? ""),
                    material: String(vi.material ?? ""),
                },
                instanceOf: String(a.instanceOf ?? ""),
                vars: parseVars(a.vars),
                components: parseComponents(a.components),
            };
        });

    const cam = (root.camera as Record<string, unknown>) ?? {};
    return {
        id:     String(root.id ?? id),
        domain: String(root.domain ?? "level"),
        mount:  String(root.mount ?? "/level"),
        actors,
        camera: {
            mode:     String(cam.mode ?? "orbit"),
            target:   asVec3(cam.target) ?? [0, 0, 0],
            distance: typeof cam.distance === "number" ? cam.distance : 8,
            fov_deg:  typeof cam.fov_deg  === "number" ? cam.fov_deg  : 50,
        },
        app,
    };
}

function parseVars(raw: unknown): ActorSummary["vars"] {
    if (!Array.isArray(raw)) return [];
    return raw
        .filter((v): v is Record<string, unknown> => typeof v === "object" && v !== null)
        .map((v) => ({
            name:  String(v.name ?? ""),
            type:  String(v.type ?? ""),
            value: String(v.value ?? ""),
        }));
}

function parseComponents(raw: unknown): ActorSummary["components"] {
    if (!Array.isArray(raw)) return [];
    return raw
        .filter((c): c is Record<string, unknown> => typeof c === "object" && c !== null)
        .map((c) => ({
            type:  String(c.type ?? ""),
            props: Object.entries(c)
                .filter(([k]) => k !== "type")
                .map(([k, v]) => [k, String(v)] as [string, string]),
        }));
}

function asVec3(v: unknown): [number, number, number] | null {
    if (!Array.isArray(v) || v.length < 3) return null;
    return [Number(v[0]), Number(v[1]), Number(v[2])];
}

function asVec4(v: unknown): [number, number, number, number] | null {
    if (!Array.isArray(v) || v.length < 4) return null;
    return [Number(v[0]), Number(v[1]), Number(v[2]), Number(v[3])];
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
