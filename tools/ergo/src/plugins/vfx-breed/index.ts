/// VFX Breed plugin — evolve a ParticleEffectConfig by picking favourites.
///
/// The browser shows a generation of variants; the human picks one or more,
/// and POST /vfx-breed/api/generation breeds the next generation from them.
/// Fitness is the human eye — there is no automatic quality metric. The
/// server is stateless: the history lives in the browser.
///
/// Routes:
///   POST /api/generation  parents + selection + direction + strength + seed + count -> children
///   GET  /api/seeds       starting points (default effect + vfx-suggest catalog)
///   GET  /api/genes       the gene table (ids / ranges) for the UI
///   GET  /api/health      direction advisor status (breeding works without it)

import { Hono } from "hono";
import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";
import { makeAdvisor, type DirectionAdvisor } from "./advisor.js";
import { parseGenerationRequest } from "./request.js";
import { runGeneration } from "./generation.js";
import { listSeeds } from "./seeds.js";
import { GENES } from "./genes.js";

export function createVfxBreedPlugin(advisor: DirectionAdvisor): Plugin {
    return {
        id:          "vfx-breed",
        title:       "VFX Breed",
        icon:        "🧬",
        description: "Evolve particle effects by picking favourites from a generation of variants.",
        staticRoot:  "./src/plugins/vfx-breed/ui",

        routes(ctx: PluginContext) {
            const app = new Hono();

            app.get("/api/seeds", (c) => c.json(listSeeds()));

            app.get("/api/genes", (c) =>
                c.json(GENES.map(({ id, label, range, scale, wrap }) => ({ id, label, range, scale, wrap })))
            );

            app.post("/api/generation", async (c) => {
                const body = await c.req.json().catch(() => null);
                const parsed = parseGenerationRequest(body);
                if (!parsed.ok) return c.json({ ok: false, err: parsed.err }, 400);

                const result = await runGeneration(parsed.req, advisor);
                if (result.direction.status === "failed") {
                    ctx.log("warn", `vfx-breed: direction ignored: ${result.direction.reason}`);
                }
                ctx.log("info",
                    `vfx-breed: gen ${result.generation} from ${parsed.req.parents.length} parent(s) ` +
                    `→ ${result.children.length} children (seed ${result.seed}, direction ${result.direction.status})`);
                return c.json({ ok: true, ...result });
            });

            app.get("/api/health", (c) => c.json({ ok: true, advisor: advisor.health() }));

            return app;
        },

        health() {
            return { ok: true, advisor: advisor.health() };
        },
    };
}

const factory: PluginFactory = () => createVfxBreedPlugin(makeAdvisor());

export default factory;
