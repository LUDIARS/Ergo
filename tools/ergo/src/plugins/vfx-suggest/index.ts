/// VFX Suggest plugin — generate Ergo ParticleEffectConfig presets from game + scene context.
/// One POST to /vfx-suggest/api/generate calls the configured LLM backend (CLI or SDK)
/// and returns ready-to-use ParticleEffectConfig JSON.
/// The browser UI connects directly to the particle plugin's WS for live preview.

import { Hono } from "hono";
import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";
import { CATALOG } from "./catalog.js";
import { makeGenerator } from "./generator.js";
import { GAME_LABELS, isGameId } from "./schema.js";

const factory: PluginFactory = () => {
    const gen = makeGenerator();

    const plugin: Plugin = {
        id:          "vfx-suggest",
        title:       "VFX Suggest",
        icon:        "🎆",
        description: "Generate ParticleEffectConfig presets from game + scene context via LLM.",
        staticRoot:  "./src/plugins/vfx-suggest/ui",

        routes(ctx: PluginContext) {
            const app = new Hono();

            app.get("/api/games", (c) =>
                c.json(Object.entries(GAME_LABELS).map(([id, label]) => ({ id, label })))
            );

            app.get("/api/catalog", (c) => {
                const game = c.req.query("game");
                const list = game
                    ? CATALOG.filter((_, i) => i < 8)  // all entries; game filtering is client-side
                    : CATALOG;
                return c.json(list);
            });

            app.post("/api/generate", async (c) => {
                const body = await c.req.json().catch(() => null) as Record<string, unknown> | null;
                if (!body || typeof body !== "object") {
                    return c.json({ ok: false, err: "request body must be JSON" }, 400);
                }

                const { game, scene, count } = body;

                if (!isGameId(game)) {
                    return c.json({ ok: false, err: `game must be one of: ${Object.keys(GAME_LABELS).join(", ")}` }, 400);
                }
                if (typeof scene !== "string" || !scene.trim()) {
                    return c.json({ ok: false, err: "scene must be a non-empty string" }, 400);
                }

                const h = gen.health();
                if (!h.ok) {
                    return c.json({ ok: false, err: `backend not ready: ${h.reason ?? "unknown"}` }, 503);
                }

                const n = Math.min(4, Math.max(1, Number.isFinite(+String(count)) ? +String(count) : 3));

                try {
                    const presets = await gen.generate({ game, scene: scene.trim() }, n);
                    ctx.log("info", `vfx-suggest: ${game} "${scene.trim()}" → ${presets.length} presets`);
                    return c.json({ ok: true, presets });
                } catch (e) {
                    ctx.log("error", `vfx-suggest generate failed: ${String(e)}`);
                    return c.json({ ok: false, err: String(e) }, 502);
                }
            });

            app.get("/api/health", (c) => c.json(gen.health()));

            return app;
        },

        health() {
            return gen.health() as unknown as { ok: boolean; [k: string]: unknown };
        },
    };

    return plugin;
};

export default factory;
