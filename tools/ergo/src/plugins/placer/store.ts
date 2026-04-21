/// File-backed persistence for the placer plugin.
///
/// Single JSON file, atomic writes via tmp + rename. Path is taken
/// from `ERGO_PLACER_FILE` env var; otherwise defaults to
/// `placer-data.json` relative to the ergo server cwd.

import { existsSync } from "node:fs";
import { readFile, writeFile, rename, mkdir } from "node:fs/promises";
import { dirname, resolve } from "node:path";

import {
    emptyStore,
    normaliseStore,
    type Store,
} from "./schema.js";

export class PlacerStore {
    private readonly path: string;
    private cache: Store | null = null;

    constructor(path?: string) {
        const p = path ?? process.env.ERGO_PLACER_FILE ?? "placer-data.json";
        this.path = resolve(p);
    }

    filePath(): string { return this.path; }

    async load(): Promise<Store> {
        if (!existsSync(this.path)) {
            this.cache = emptyStore();
            return this.cache;
        }
        const text = await readFile(this.path, "utf8");
        try {
            const parsed = JSON.parse(text);
            this.cache = normaliseStore(parsed);
            return this.cache;
        } catch (err) {
            throw new Error(`${this.path}: ${(err as Error).message}`);
        }
    }

    async save(next: Store): Promise<void> {
        const validated = normaliseStore(next);
        this.cache = validated;
        const parent = dirname(this.path);
        if (!existsSync(parent)) await mkdir(parent, { recursive: true });
        const tmp = this.path + ".tmp";
        await writeFile(tmp, JSON.stringify(validated, null, 2) + "\n", "utf8");
        await rename(tmp, this.path);
    }

    /// Read-only snapshot. Loads on first call.
    async snapshot(): Promise<Store> {
        return this.cache ?? (await this.load());
    }

    /// Discard cached copy (test helper / manual reload).
    invalidate(): void { this.cache = null; }
}
