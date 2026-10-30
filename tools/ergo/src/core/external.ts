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
///   PrivateGame -> tools/game-web/plugins/{spawn,skill}
///   AdventureCube -> tools/ac-web/plugins/{placer,terrain,acstage}
///
/// A broken pack entry is logged and skipped — it never aborts startup.
/// This module only imports the entries; calling the factories, validating
/// the returned plugin and rejecting duplicate ids happens in
/// core/server.ts `instantiatePlugins()`, under the same rule.

import { existsSync } from "node:fs";
import { readdir } from "node:fs/promises";
import { delimiter, join, resolve } from "node:path";
import { pathToFileURL } from "node:url";

import type { PluginFactory } from "./plugin.js";

/** A factory imported from a plugin pack, with the file it came from. */
export interface ExternalFactory {
    factory: PluginFactory;
    /** Absolute path of the pack entry (`<pack>/<name>/index.{js,ts}`). */
    source:  string;
}

/// Import a pack entry by absolute path.
///
/// The import goes through a one-line `data:` module instead of a direct
/// `import()` from this file. Under tsx (`npm run serve` / `dev`) this file
/// is a .ts module, and tsx resolves a "x.js" request from a .ts importer to
/// a sibling "x.ts" first — which would silently load the source instead of
/// the compiled entry that loadPack() chose. A `data:` importer is not .ts,
/// so the chosen file is the one loaded in every host (tsx, Electron dist/).
async function importEntry(file: string): Promise<Record<string, unknown>> {
    const shim = `export * as ns from ${JSON.stringify(pathToFileURL(file).href)};`;
    const { ns } = await import(`data:text/javascript,${encodeURIComponent(shim)}`);
    return ns as Record<string, unknown>;
}

/// Load every plugin under a single pack directory.
export async function loadPack(dir: string): Promise<ExternalFactory[]> {
    const out: ExternalFactory[] = [];
    let entries;
    try {
        entries = await readdir(dir, { withFileTypes: true });
    } catch (err) {
        console.warn(`[ergo] plugin dir not readable: ${dir} (${(err as Error).message})`);
        return out;
    }
    // Name order, so "first id wins" does not depend on the file system.
    entries.sort((a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0));
    for (const ent of entries) {
        if (!ent.isDirectory()) continue;
        const base  = join(dir, ent.name);
        // Prefer compiled .js, fall back to .ts (loaded via tsx).
        const entry = ["index.js", "index.ts"]
            .map((f) => join(base, f))
            .find((p) => existsSync(p));
        if (!entry) continue;
        try {
            const mod     = await importEntry(entry);
            const factory = (mod.default ?? mod.factory) as PluginFactory | undefined;
            if (typeof factory !== "function") {
                console.warn(
                    `[ergo] external plugin '${ent.name}': no default PluginFactory export — skipped`,
                );
                continue;
            }
            out.push({ factory, source: entry });
            console.log(`[ergo] external plugin loaded: ${ent.name}  (${entry})`);
        } catch (err) {
            console.error(
                `[ergo] external plugin '${ent.name}' failed to load: ${(err as Error).message}`,
            );
        }
    }
    return out;
}

/// Collect plugin factories from every directory in `raw`
/// (`ERGO_PLUGIN_DIR` by default).
export async function loadExternalFactories(
    raw: string | undefined = process.env.ERGO_PLUGIN_DIR,
): Promise<ExternalFactory[]> {
    if (!raw) return [];
    const dirs = raw.split(delimiter).map((d) => d.trim()).filter(Boolean);
    const all: ExternalFactory[] = [];
    for (const d of dirs) {
        all.push(...(await loadPack(resolve(d))));
    }
    return all;
}
