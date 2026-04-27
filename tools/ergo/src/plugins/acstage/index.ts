/// AC Stage plugin — direct editor for AdventureCube stage JSON files.
///
/// Lives entirely inside `tools/ergo` and reads/writes one JSON file per
/// stage under `<AC>/data/master_data/stages/`. The AC executable picks
/// up changes on the next launch (no live hot-reload yet — see the
/// `placement_loader` / `enemy_loader` startup paths).
///
/// REST surface (all under `/acstage/api/*`):
///   GET    /meta              { dir, exists, schema_version, allowed_categories, allowed_enemy_types }
///   GET    /stages            list { stage_id }
///   GET    /stages/:id        full stage JSON (normalised)
///   PUT    /stages/:id        upsert one stage JSON (body = StageFile, full replace)
///   POST   /stages/:id        new empty stage (body optional, `from` clones an existing id)
///   DELETE /stages/:id        remove one stage
///
/// The WebSocket endpoint is reserved (no-op handshake) so a future
/// hot-reload path on the AC side can push updates without a protocol
/// change.

import { Hono } from "hono";
import { WebSocket as WS } from "ws";

import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";
import {
    ENEMY_TYPES,
    FIELD_CATEGORIES,
    SCHEMA_VERSION,
    emptyStage,
    normaliseStageFile,
    type StageFile,
} from "./schema.js";
import { ACStageStore } from "./store.js";

const factory: PluginFactory = () => {
    const store = new ACStageStore();
    const wsClients = new Set<WS>();

    function broadcast(op: "reload", stage_id?: string) {
        const text = JSON.stringify(stage_id ? { op, stage_id } : { op });
        for (const ws of wsClients) if (ws.readyState === WS.OPEN) ws.send(text);
    }

    const plugin: Plugin = {
        id:         "acstage",
        title:      "AC Stage Editor",
        icon:       "🎮",
        description:
            "AdventureCube stage editor. Edits data/master_data/stages/<stage_id>.json directly (fields / placements / enemies). AC re-reads on launch.",
        staticRoot: "./src/plugins/acstage/ui",

        routes(ctx: PluginContext) {
            const app = new Hono();

            app.get("/api/meta", async (c) => {
                const h = await store.health();
                return c.json({
                    schema_version:       SCHEMA_VERSION,
                    dir:                  h.dir,
                    exists:               h.exists,
                    stages:               h.stages,
                    allowed_categories:   FIELD_CATEGORIES,
                    allowed_enemy_types:  ENEMY_TYPES,
                });
            });

            app.get("/api/stages", async (c) => {
                const ids = await store.list();
                return c.json({ stages: ids.map((stage_id) => ({ stage_id })) });
            });

            app.get("/api/stages/:id", async (c) => {
                const id = c.req.param("id");
                try {
                    const stage = await store.load(id);
                    return c.json({ ok: true, stage });
                } catch (err) {
                    return c.json({ ok: false, error: (err as Error).message }, 404);
                }
            });

            app.put("/api/stages/:id", async (c) => {
                const id = c.req.param("id");
                const body = await c.req.json().catch(() => null);
                if (!body || typeof body !== "object") {
                    return c.json({ ok: false, error: "invalid JSON" }, 400);
                }
                try {
                    const incoming = normaliseStageFile({ ...(body as object), stage_id: id });
                    await store.save(incoming);
                    ctx.log("info", `[acstage] saved ${store.fileFor(id)}`);
                    broadcast("reload", id);
                    return c.json({ ok: true, stage: incoming });
                } catch (err) {
                    return c.json({ ok: false, error: (err as Error).message }, 400);
                }
            });

            app.post("/api/stages/:id", async (c) => {
                const id = c.req.param("id");
                const body = await c.req.json().catch(() => ({}));
                const fromId = body && typeof body === "object" && "from" in body
                    ? String((body as Record<string, unknown>).from ?? "").trim()
                    : "";
                let next: StageFile;
                if (fromId) {
                    try { next = await store.load(fromId); }
                    catch (err) { return c.json({ ok: false, error: (err as Error).message }, 400); }
                    next = { ...next, stage_id: id };
                } else {
                    next = emptyStage(id);
                }
                try {
                    await store.save(next);
                    broadcast("reload", id);
                    return c.json({ ok: true, stage: next });
                } catch (err) {
                    return c.json({ ok: false, error: (err as Error).message }, 400);
                }
            });

            app.delete("/api/stages/:id", async (c) => {
                const id = c.req.param("id");
                const removed = await store.remove(id);
                if (!removed) return c.json({ ok: false, error: "not found" }, 404);
                broadcast("reload");
                return c.json({ ok: true });
            });

            return app;
        },

        onUpgrade(_req, ws: WS, _ctx) {
            wsClients.add(ws);
            ws.on("message", () => { /* reserved for future live-bind pushes */ });
            ws.on("close", () => wsClients.delete(ws));
            ws.on("error", () => wsClients.delete(ws));
        },

        health() {
            // health() in the contract is sync; we expose async `dir/exists`
            // via /api/meta, and return only the synchronously-known bits
            // here so /api/health stays cheap.
            return {
                ok:      true,
                clients: wsClients.size,
                dir:     store.rootDir(),
                exists:  store.exists(),
            };
        },
    };

    return plugin;
};

export default factory;
