/// Terrain plugin — ground-field stage composer.
///
/// Lives under `tools/ergo`. The UI lets authors compose an ordered
/// list of Fields (grass / soil / ice / cobble) for each Stage; the
/// concrete SVG pattern for a given Field is picked deterministically
/// at render time from `FIELD_PATTERNS[category]`.
///
/// REST surface (under `/terrain/api/*`):
///   GET    /meta                list of categories + patterns
///   GET    /store               full store snapshot
///   POST   /store/import        replace store
///   GET    /stages              all stages
///   PUT    /stages/:id          upsert stage
///   DELETE /stages/:id          remove stage
///   POST   /new/stage           { id? } -> makeStage skeleton
///   POST   /new/field           { id? category? } -> makeField skeleton
///   GET    /patterns/:category  list SVG filenames for a category
///
/// The WebSocket endpoint broadcasts `{ op: "reload" }` whenever the
/// store changes so co-opened UIs stay in sync (same pattern as
/// placer).

import { Hono } from "hono";
import { WebSocket as WS } from "ws";

import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";
import {
    FIELD_CATEGORIES,
    FIELD_PATTERNS,
    SCHEMA_VERSION,
    makeField,
    makeStage,
    normaliseStage,
    type FieldCategory,
    type Stage,
    type Store,
} from "./schema.js";
import { TerrainStore } from "./store.js";

const factory: PluginFactory = () => {
    const store = new TerrainStore();
    const wsClients = new Set<WS>();

    async function commit(next: Store): Promise<void> {
        await store.save(next);
        broadcast("reload");
    }

    function broadcast(op: "reload") {
        const text = JSON.stringify({ op });
        for (const ws of wsClients) if (ws.readyState === WS.OPEN) ws.send(text);
    }

    const plugin: Plugin = {
        id:         "terrain",
        title:      "Terrain",
        icon:       "🌱",
        description:
            "Compose stages from square ground fields (草原 / 土 / 氷 / 石畳). Concrete SVG pattern is picked deterministically at render time.",
        staticRoot: "./src/plugins/terrain/ui",

        routes(_ctx: PluginContext) {
            const app = new Hono();

            app.get("/api/meta", (c) =>
                c.json({
                    version:    SCHEMA_VERSION,
                    storePath:  store.filePath(),
                    categories: FIELD_CATEGORIES,
                    patterns:   FIELD_PATTERNS,
                })
            );

            app.get("/api/health", async (c) => {
                const snap = await store.snapshot();
                return c.json({
                    ok:       true,
                    stages:   snap.stages.length,
                    fields:   snap.stages.reduce((n, s) => n + s.fields.length, 0),
                    clients:  wsClients.size,
                });
            });

            // --- bulk -----------------------------------------------
            app.get("/api/store", async (c) => c.json(await store.snapshot()));

            app.post("/api/store/import", async (c) => {
                const body = await c.req.json().catch(() => null);
                if (!body) return c.json({ ok: false, error: "invalid JSON" }, 400);
                try {
                    await commit(body as Store);
                    return c.json({ ok: true });
                } catch (err) {
                    return c.json({ ok: false, error: (err as Error).message }, 400);
                }
            });

            // --- stages ---------------------------------------------
            app.get("/api/stages", async (c) => c.json((await store.snapshot()).stages));

            app.put("/api/stages/:id", async (c) => {
                const id   = c.req.param("id");
                const body = await c.req.json().catch(() => null);
                if (!body || typeof body !== "object") {
                    return c.json({ ok: false, error: "invalid JSON" }, 400);
                }
                try {
                    const incoming = normaliseStage({ ...(body as object), id });
                    const snap = await store.snapshot();
                    const next: Store = structuredClone(snap);
                    upsertById(next.stages, incoming);
                    await commit(next);
                    return c.json({ ok: true, stage: incoming });
                } catch (err) {
                    return c.json({ ok: false, error: (err as Error).message }, 400);
                }
            });

            app.delete("/api/stages/:id", async (c) => {
                const id = c.req.param("id");
                const snap = await store.snapshot();
                const next: Store = structuredClone(snap);
                const before = next.stages.length;
                next.stages = next.stages.filter((s) => s.id !== id);
                if (next.stages.length === before) {
                    return c.json({ ok: false, error: "not found" }, 404);
                }
                await commit(next);
                return c.json({ ok: true });
            });

            // --- convenience skeletons ------------------------------
            app.post("/api/new/stage", async (c) => {
                const body = await c.req.json().catch(() => ({}));
                const snap = await store.snapshot();
                const id = String((body as Record<string, unknown>)?.id ?? "").trim()
                    || nextStageId(snap);
                return c.json(makeStage(id));
            });

            app.post("/api/new/field", async (c) => {
                const body = (await c.req.json().catch(() => ({}))) as Record<string, unknown>;
                const id = String(body?.id ?? "").trim() || "f_" + Date.now().toString(36);
                const rawCat = String(body?.category ?? "grass");
                const category: FieldCategory = FIELD_CATEGORIES.includes(rawCat as FieldCategory)
                    ? (rawCat as FieldCategory) : "grass";
                return c.json(makeField(id, category));
            });

            // --- patterns (UI uses this to show a preview) ----------
            app.get("/api/patterns/:category", (c) => {
                const rawCat = c.req.param("category");
                if (!FIELD_CATEGORIES.includes(rawCat as FieldCategory)) {
                    return c.json({ ok: false, error: "unknown category" }, 400);
                }
                return c.json(FIELD_PATTERNS[rawCat as FieldCategory]);
            });

            return app;
        },

        onUpgrade(_req, ws: WS, _ctx) {
            wsClients.add(ws);
            ws.on("message", () => { /* reserved */ });
            ws.on("close", () => wsClients.delete(ws));
            ws.on("error", () => wsClients.delete(ws));
        },

        health() {
            const snap = store.snapshot();
            if (snap instanceof Promise) return { ok: true, clients: wsClients.size };
            const s = snap as Store;
            return {
                ok:      true,
                stages:  s.stages.length,
                fields:  s.stages.reduce((n, st) => n + st.fields.length, 0),
                clients: wsClients.size,
            };
        },
    };

    // Warm the cache on startup so the first GET /store is instant.
    store.load().catch((err) => console.warn("[terrain] initial load:", err));

    return plugin;
};

function upsertById<T extends { id: string }>(arr: T[], next: T): void {
    const idx = arr.findIndex((x) => x.id === next.id);
    if (idx >= 0) arr[idx] = next;
    else          arr.push(next);
}

function nextStageId(snap: Store): string {
    const pool = new Set(snap.stages.map((s) => s.id));
    for (let i = 1; i < 10000; ++i) {
        const cand = `stage_${String(i).padStart(2, "0")}`;
        if (!pool.has(cand)) return cand;
    }
    return `stage_${Date.now()}`;
}

export default factory;
