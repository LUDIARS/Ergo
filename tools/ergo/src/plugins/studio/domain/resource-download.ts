// @implements SPEC-ergo-studio-resources
/// Curare の取得計画を実行して、プロジェクトの取得先フォルダへ書く。
///
/// - 手元の同名ファイルのハッシュで判定する (resource-sync.ts)。上書きはしない。
/// - 一時ファイルに書き、内容ハッシュが計画と一致したときだけ名前を付け替える。
///   一致しなければ一時ファイルを消して失敗にする (壊れた取得物を残さない)。

import { createHash } from "node:crypto";
import { createReadStream, createWriteStream, existsSync } from "node:fs";
import { mkdir, readdir, rename, rm, stat } from "node:fs/promises";
import { join } from "node:path";
import { Readable } from "node:stream";
import { pipeline } from "node:stream/promises";

import type { CuPlan } from "../clients/curare.js";
import { decideSyncAction, safeFileName, toContentHash } from "./resource-sync.js";

export interface SyncItemResult {
    name:     string;
    versionId: string;
    result:   "downloaded" | "skip_same" | "conflict" | "unavailable" | "failed" | "invalid_name";
    detail?:  string;
}

async function hashFile(path: string): Promise<string> {
    const h = createHash("sha256");
    await pipeline(createReadStream(path), h);
    return toContentHash(h.digest("hex"));
}

/// 取得先フォルダ直下のファイルの内容ハッシュ (計画に渡して取得を省く)。
export async function localHashes(dir: string, limit = 1000): Promise<string[]> {
    if (!existsSync(dir)) return [];
    const out: string[] = [];
    for (const ent of await readdir(dir, { withFileTypes: true })) {
        if (!ent.isFile() || ent.name.endsWith(".part")) continue;
        out.push(await hashFile(join(dir, ent.name)));
        if (out.length >= limit) break;
    }
    return out;
}

export async function executePlan(plan: CuPlan, dir: string, timeoutMs: number): Promise<SyncItemResult[]> {
    await mkdir(dir, { recursive: true });
    const results: SyncItemResult[] = [];
    for (const item of plan.items) {
        const name = safeFileName(item.name);
        if (!name) {
            results.push({ name: item.name, versionId: item.versionId, result: "invalid_name" });
            continue;
        }
        const path = join(dir, name);
        const local = existsSync(path) && (await stat(path)).isFile() ? await hashFile(path) : null;
        const decision = decideSyncAction(item, local);
        if (decision.action !== "download") {
            results.push({ name, versionId: item.versionId, result: decision.action });
            continue;
        }
        const tmp = `${path}.${item.versionId.slice(0, 8)}.part`;
        try {
            const res = await fetch(item.downloadUrl!, { signal: AbortSignal.timeout(timeoutMs) });
            if (!res.ok || !res.body) throw new Error(`HTTP ${res.status}`);
            await pipeline(Readable.fromWeb(res.body as import("node:stream/web").ReadableStream), createWriteStream(tmp));
            const got = await hashFile(tmp);
            if (got !== item.contentHash) throw new Error("内容ハッシュが計画と一致しません");
            await rename(tmp, path);
            results.push({ name, versionId: item.versionId, result: "downloaded" });
        } catch (err) {
            await rm(tmp, { force: true });
            results.push({ name, versionId: item.versionId, result: "failed", detail: (err as Error).message });
        }
    }
    return results;
}
