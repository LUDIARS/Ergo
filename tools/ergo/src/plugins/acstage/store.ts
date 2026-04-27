/// File-backed persistence for the AC stage plugin.
///
/// Reads / writes one JSON file per stage under a configurable
/// directory. Lookup precedence:
///   1. constructor `dir` argument
///   2. `ERGO_ACSTAGE_DIR` env var
///   3. probe a few likely locations for the AC repo's
///      `data/master_data/stages/`
///
/// Files are stored as `<stage_id>.json` (one file per stage). Atomic
/// writes via tmp + rename so an interrupted save can't half-write a
/// stage the game is about to load.

import { existsSync } from "node:fs";
import { mkdir, readFile, readdir, rename, stat, unlink, writeFile } from "node:fs/promises";
import { dirname, isAbsolute, join, resolve } from "node:path";

import { normaliseStageFile, type StageFile } from "./schema.js";

const PROBE_RELATIVE = [
    "data/master_data/stages",
    "../data/master_data/stages",
    "../../data/master_data/stages",
    "../../../data/master_data/stages",
    "../../../../data/master_data/stages",
    // When the dev server runs from <host>/external/ergo/tools/ergo
    "../../../../AdventureCube/data/master_data/stages",
];

function discoverDir(): string {
    const env = process.env.ERGO_ACSTAGE_DIR;
    if (env && env.trim().length) {
        return isAbsolute(env) ? env : resolve(env);
    }
    for (const rel of PROBE_RELATIVE) {
        const p = resolve(rel);
        if (existsSync(p)) return p;
    }
    // Fallback: ./data/master_data/stages (will be created lazily on save).
    return resolve("data/master_data/stages");
}

export class ACStageStore {
    private readonly dir: string;

    constructor(dir?: string) {
        this.dir = dir ?? discoverDir();
    }

    /** Absolute path to the stages directory. */
    rootDir(): string { return this.dir; }

    /** Probe whether the directory exists right now (no creation). */
    exists(): boolean { return existsSync(this.dir); }

    /** Path used for a given stage id. */
    fileFor(stage_id: string): string {
        const safe = stage_id.replace(/[^A-Za-z0-9_-]/g, "_");
        return join(this.dir, `${safe}.json`);
    }

    /** List stage ids by reading directory entries. Missing dir → []. */
    async list(): Promise<string[]> {
        if (!existsSync(this.dir)) return [];
        const items = await readdir(this.dir);
        return items
            .filter((name) => name.endsWith(".json"))
            .map((name) => name.slice(0, -".json".length))
            .sort();
    }

    /** Load + normalise one stage file. Throws on missing or invalid. */
    async load(stage_id: string): Promise<StageFile> {
        const path = this.fileFor(stage_id);
        if (!existsSync(path)) throw new Error(`stage not found: ${stage_id}`);
        const text = await readFile(path, "utf8");
        let parsed: unknown;
        try {
            parsed = JSON.parse(text);
        } catch (err) {
            throw new Error(`${path}: ${(err as Error).message}`);
        }
        const stage = normaliseStageFile(parsed);
        // The on-disk stage_id is authoritative; if the JSON disagrees
        // (rare but possible after manual rename) we trust the filename
        // — the AC loader keys nothing off the in-file id.
        if (stage.stage_id !== stage_id) stage.stage_id = stage_id;
        return stage;
    }

    /** Persist one stage file (atomic). Creates parent dir on demand. */
    async save(stage: StageFile): Promise<void> {
        const validated = normaliseStageFile(stage);
        if (!existsSync(this.dir)) await mkdir(this.dir, { recursive: true });
        const path = this.fileFor(validated.stage_id);
        const parent = dirname(path);
        if (!existsSync(parent)) await mkdir(parent, { recursive: true });
        const tmp = path + ".tmp";
        await writeFile(tmp, JSON.stringify(validated, null, 2) + "\n", "utf8");
        await rename(tmp, path);
    }

    /** Remove one stage file. Missing file is a no-op (returns false). */
    async remove(stage_id: string): Promise<boolean> {
        const path = this.fileFor(stage_id);
        if (!existsSync(path)) return false;
        await unlink(path);
        return true;
    }

    /** Health snapshot — last-modified time of the dir, file count. */
    async health(): Promise<{ ok: boolean; dir: string; exists: boolean; stages: number; mtime?: number; }> {
        const exists = existsSync(this.dir);
        if (!exists) return { ok: true, dir: this.dir, exists: false, stages: 0 };
        const items = await this.list();
        const st = await stat(this.dir).catch(() => undefined);
        return {
            ok:      true,
            dir:     this.dir,
            exists:  true,
            stages:  items.length,
            mtime:   st ? st.mtimeMs : undefined,
        };
    }
}
