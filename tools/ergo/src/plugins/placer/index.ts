/// Placer plugin — block-based level designer.
///
/// Lives entirely inside `tools/ergo`; ships as one mode of the Ergo
/// desktop app alongside `particle` and `variable`. Not linked into
/// any game executable.
///
/// REST surface (all under `/placer/api/*`):
///   GET    /blocks                     list all blocks
///   PUT    /blocks/:id                 upsert a block (body = Block JSON)
///   DELETE /blocks/:id                 remove a block (stages are pruned)
///   GET    /stages                     list all stages
///   PUT    /stages/:id                 upsert a stage
///   DELETE /stages/:id                 remove a stage
///   GET    /enemies                    list enemies (sheet)
///   PUT    /enemies/:id                upsert enemy
///   DELETE /enemies/:id                remove enemy (cell refs stay — UI warns)
///   GET    /skill-blocks               list skill blocks (sheet)
///   PUT    /skill-blocks/:id           upsert skill block
///   DELETE /skill-blocks/:id           remove; enemies with this block's id get it cleared
///   GET    /store                      full store
///   POST   /store/import               replace store
///   GET    /meta                       { version, storePath, allowedRows, ... }
///
/// The WebSocket endpoint is reserved (see `onUpgrade` — currently a
/// no-op handshake) so future phases can push live updates to peer UI
/// windows without changing the API surface.

import { Hono } from "hono";
import { WebSocket as WS } from "ws";

import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";
import {
    BLOCK_COLS,
    BLOCK_ROWS_ALLOWED,
    PLACED_OBJECT_TYPES,
    SCHEMA_VERSION,
    makeBlock,
    makeEnemy,
    makeSkillBlock,
    makeStage,
    normaliseBlock,
    normaliseEnemy,
    normaliseSkillBlock,
    normaliseStage,
    resizeGrid,
    type Block,
    type BlockRows,
    type Enemy,
    type SkillBlock,
    type Stage,
    type Store,
} from "./schema.js";
import { PlacerStore } from "./store.js";

type IdPrefix = "block" | "stage" | "enemy" | "skillBlock";

const factory: PluginFactory = () => {
    const store = new PlacerStore();
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
        id:         "placer",
        title:      "Level Placer",
        icon:       "🗺",
        description:
            "Block-based level designer. Compose 3×10 / 5×10 grid blocks into stages; each cell holds Enemy / SkillBlock / SkillBox / Special entries.",
        staticRoot: "./src/plugins/placer/ui",

        routes(_ctx: PluginContext) {
            const app = new Hono();

            // --- meta ----------------------------------------------------
            app.get("/api/meta", (c) =>
                c.json({
                    version:          SCHEMA_VERSION,
                    storePath:        store.filePath(),
                    allowedRows:      BLOCK_ROWS_ALLOWED,
                    cols:             BLOCK_COLS,
                    placedObjectTypes: PLACED_OBJECT_TYPES,
                })
            );

            app.get("/api/health", async (c) =>
                c.json({ ok: true, ...countHealth(await store.snapshot()), clients: wsClients.size })
            );

            // --- bulk ----------------------------------------------------
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

            // --- blocks --------------------------------------------------
            app.get("/api/blocks", async (c) => c.json((await store.snapshot()).blocks));

            app.put("/api/blocks/:id", async (c) => {
                const id   = c.req.param("id");
                const body = await c.req.json().catch(() => null);
                if (!body || typeof body !== "object") return c.json({ ok: false, error: "invalid JSON" }, 400);
                try {
                    const incoming = normaliseBlock({ ...(body as object), id });
                    const snap  = await store.snapshot();
                    const next: Store = structuredClone(snap);
                    upsertById(next.blocks, incoming);
                    await commit(next);
                    return c.json({ ok: true, block: incoming });
                } catch (err) {
                    return c.json({ ok: false, error: (err as Error).message }, 400);
                }
            });

            app.delete("/api/blocks/:id", async (c) => {
                const id = c.req.param("id");
                const snap = await store.snapshot();
                const next: Store = structuredClone(snap);
                const before = next.blocks.length;
                next.blocks = next.blocks.filter((b) => b.id !== id);
                for (const s of next.stages) {
                    s.blocks = s.blocks.filter((bid) => bid !== id);
                }
                if (next.blocks.length === before) return c.json({ ok: false, error: "not found" }, 404);
                await commit(next);
                return c.json({ ok: true });
            });

            // --- stages --------------------------------------------------
            app.get("/api/stages", async (c) => c.json((await store.snapshot()).stages));

            app.put("/api/stages/:id", async (c) => {
                const id   = c.req.param("id");
                const body = await c.req.json().catch(() => null);
                if (!body || typeof body !== "object") return c.json({ ok: false, error: "invalid JSON" }, 400);
                try {
                    const incoming = normaliseStage({ ...(body as object), id });
                    const snap  = await store.snapshot();
                    const next: Store = structuredClone(snap);
                    const knownBlockIds = new Set(next.blocks.map((b) => b.id));
                    incoming.blocks = incoming.blocks.filter((bid) => knownBlockIds.has(bid));
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
                if (next.stages.length === before) return c.json({ ok: false, error: "not found" }, 404);
                await commit(next);
                return c.json({ ok: true });
            });

            // --- enemies -------------------------------------------------
            app.get("/api/enemies", async (c) => c.json((await store.snapshot()).enemies));

            app.put("/api/enemies/:id", async (c) => {
                const id   = c.req.param("id");
                const body = await c.req.json().catch(() => null);
                if (!body || typeof body !== "object") return c.json({ ok: false, error: "invalid JSON" }, 400);
                try {
                    const incoming = normaliseEnemy({ ...(body as object), id });
                    const snap = await store.snapshot();
                    if (incoming.skillBlockId && !snap.skillBlocks.some((sb) => sb.id === incoming.skillBlockId)) {
                        return c.json({ ok: false, error: `unknown skillBlock id: ${incoming.skillBlockId}` }, 400);
                    }
                    const next: Store = structuredClone(snap);
                    upsertById(next.enemies, incoming);
                    await commit(next);
                    return c.json({ ok: true, enemy: incoming });
                } catch (err) {
                    return c.json({ ok: false, error: (err as Error).message }, 400);
                }
            });

            app.delete("/api/enemies/:id", async (c) => {
                const id = c.req.param("id");
                const snap = await store.snapshot();
                const next: Store = structuredClone(snap);
                const before = next.enemies.length;
                next.enemies = next.enemies.filter((e) => e.id !== id);
                if (next.enemies.length === before) return c.json({ ok: false, error: "not found" }, 404);
                // Cells referencing this enemy keep their entries; UI displays
                // "(missing)" so designers can re-point them rather than silently
                // losing placements.
                await commit(next);
                return c.json({ ok: true });
            });

            // --- skill blocks --------------------------------------------
            app.get("/api/skill-blocks", async (c) => c.json((await store.snapshot()).skillBlocks));

            app.put("/api/skill-blocks/:id", async (c) => {
                const id   = c.req.param("id");
                const body = await c.req.json().catch(() => null);
                if (!body || typeof body !== "object") return c.json({ ok: false, error: "invalid JSON" }, 400);
                try {
                    const incoming = normaliseSkillBlock({ ...(body as object), id });
                    const snap = await store.snapshot();
                    const next: Store = structuredClone(snap);
                    upsertById(next.skillBlocks, incoming);
                    await commit(next);
                    return c.json({ ok: true, skillBlock: incoming });
                } catch (err) {
                    return c.json({ ok: false, error: (err as Error).message }, 400);
                }
            });

            app.delete("/api/skill-blocks/:id", async (c) => {
                const id = c.req.param("id");
                const snap = await store.snapshot();
                const next: Store = structuredClone(snap);
                const before = next.skillBlocks.length;
                next.skillBlocks = next.skillBlocks.filter((sb) => sb.id !== id);
                if (next.skillBlocks.length === before) return c.json({ ok: false, error: "not found" }, 404);
                // Clear enemies that pointed at this skill block.
                for (const e of next.enemies) {
                    if (e.skillBlockId === id) e.skillBlockId = "";
                }
                await commit(next);
                return c.json({ ok: true });
            });

            // --- helpers used by the UI ---------------------------------

            app.post("/api/blocks/:id/resize", async (c) => {
                const id   = c.req.param("id");
                const body = await c.req.json().catch(() => null);
                if (!body || typeof body !== "object") return c.json({ ok: false, error: "invalid JSON" }, 400);
                const rowsIn = Number((body as Record<string, unknown>).rows);
                const nextRows: BlockRows = rowsIn === 5 ? 5 : 3;
                const snap = await store.snapshot();
                const current = snap.blocks.find((b) => b.id === id);
                if (!current) return c.json({ ok: false, error: "not found" }, 404);
                const resized: Block = {
                    ...current,
                    rows: nextRows,
                    grid: resizeGrid(current.grid, nextRows),
                };
                const next: Store = structuredClone(snap);
                upsertById(next.blocks, resized);
                await commit(next);
                return c.json({ ok: true, block: resized });
            });

            // Convenience creators — return skeleton objects without saving.
            app.post("/api/new/block", async (c) => {
                const body = await c.req.json().catch(() => ({}));
                const snap = await store.snapshot();
                const id   = String((body as Record<string, unknown>)?.id ?? "").trim() || nextIdFor(snap, "block");
                const rows = Number((body as Record<string, unknown>)?.rows) === 5 ? 5 : 3;
                return c.json(makeBlock(id, rows as BlockRows));
            });
            app.post("/api/new/stage", async (c) => {
                const body = await c.req.json().catch(() => ({}));
                const snap = await store.snapshot();
                const id   = String((body as Record<string, unknown>)?.id ?? "").trim() || nextIdFor(snap, "stage");
                return c.json(makeStage(id));
            });
            app.post("/api/new/enemy", async (c) => {
                const body = await c.req.json().catch(() => ({}));
                const snap = await store.snapshot();
                const id   = String((body as Record<string, unknown>)?.id ?? "").trim() || nextIdFor(snap, "enemy");
                return c.json(makeEnemy(id));
            });
            app.post("/api/new/skill-block", async (c) => {
                const body = await c.req.json().catch(() => ({}));
                const snap = await store.snapshot();
                const id   = String((body as Record<string, unknown>)?.id ?? "").trim() || nextIdFor(snap, "skillBlock");
                return c.json(makeSkillBlock(id));
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
            const snap = store.snapshot();
            if (snap instanceof Promise) return { ok: true, clients: wsClients.size };
            return { ok: true, ...countHealth(snap as Store), clients: wsClients.size };
        },
    };

    // Kick off an async load in the background so the first request is warm.
    store.load().catch((err) => console.warn("[placer] initial load:", err));

    return plugin;
};

function upsertById<T extends { id: string }>(arr: T[], next: T): void {
    const idx = arr.findIndex((x) => x.id === next.id);
    if (idx >= 0) arr[idx] = next;
    else          arr.push(next);
}

function nextIdFor(snap: Store, prefix: IdPrefix): string {
    const pool =
        prefix === "block"      ? snap.blocks.map((b) => b.id) :
        prefix === "stage"      ? snap.stages.map((s) => s.id) :
        prefix === "enemy"      ? snap.enemies.map((e) => e.id) :
                                  snap.skillBlocks.map((sb) => sb.id);
    for (let i = 1; i < 10000; ++i) {
        const cand = `${prefix}_${String(i).padStart(2, "0")}`;
        if (!pool.includes(cand)) return cand;
    }
    return `${prefix}_${Date.now()}`;
}

function countHealth(snap: Store) {
    return {
        blocks:      snap.blocks.length,
        stages:      snap.stages.length,
        enemies:     snap.enemies.length,
        skillBlocks: snap.skillBlocks.length,
    };
}

export default factory;
