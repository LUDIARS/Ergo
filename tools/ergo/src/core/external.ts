/// External plugin loading.
///
/// On top of the built-in plugins (registry.ts), the tool loads plugins
/// from directories named in the `ERGO_PLUGIN_DIR` environment variable
/// (OS-path-separated — ";" on Windows, ":" elsewhere).
///
/// Each named directory is a *plugin pack*. Every immediate subdirectory
/// that has an `index.js` or `index.ts` is dynamic-imported and its
/// default export is expected to be a `PluginFactory`. This lets a game
/// repo keep its own editor plugins without forking the shared tool:
///   KuzuSurvivors -> tools/kzs-web/plugins/{spawn,skill}
///   AdventureCube -> tools/ac-web/plugins/{placer,terrain,acstage}
///
/// A broken pack entry is logged and skipped — it never aborts startup.

import { existsSync } from "node:fs";
import { readdir } from "node:fs/promises";
import { delimiter, join, resolve } from "node:path";
import { pathToFileURL } from "node:url";

import type { PluginFactory } from "./plugin.js";

/// Load every plugin under a single pack directory.
async function loadPack(dir: string): Promise<PluginFactory[]> {
    const out: PluginFactory[] = [];
    let entries;
    try {
        entries = await readdir(dir, { withFileTypes: true });
    } catch (err) {
        console.warn(`[ergo] plugin dir not readable: ${dir} (${(err as Error).message})`);
        return out;
    }
    for (const ent of entries) {
        if (!ent.isDirectory()) continue;
        const base  = join(dir, ent.name);
        // Prefer compiled .js, fall back to .ts (loaded via tsx).
        const entry = ["index.js", "index.ts"]
            .map((f) => join(base, f))
            .find((p) => existsSync(p));
        if (!entry) continue;
        try {
            const mod     = await import(pathToFileURL(entry).href);
            const factory = (mod.default ?? mod.factory) as PluginFactory | undefined;
            if (typeof factory !== "function") {
                console.warn(
                    `[ergo] external plugin '${ent.name}': no default PluginFactory export — skipped`,
                );
                continue;
            }
            out.push(factory);
            console.log(`[ergo] external plugin loaded: ${ent.name}  (${entry})`);
        } catch (err) {
            console.error(
                `[ergo] external plugin '${ent.name}' failed to load: ${(err as Error).message}`,
            );
        }
    }
    return out;
}

/// Collect plugin factories from every directory in `ERGO_PLUGIN_DIR`.
export async function loadExternalFactories(): Promise<PluginFactory[]> {
    const raw = process.env.ERGO_PLUGIN_DIR;
    if (!raw) return [];
    const dirs = raw.split(delimiter).map((d) => d.trim()).filter(Boolean);
    const all: PluginFactory[] = [];
    for (const d of dirs) {
        all.push(...(await loadPack(resolve(d))));
    }
    return all;
}
