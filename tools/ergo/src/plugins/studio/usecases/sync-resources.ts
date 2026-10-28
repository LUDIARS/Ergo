// @implements SPEC-ergo-studio-resources
/// リソース同期 — Curare で選んだ版を、Cc 登録プロジェクトの repo 内のフォルダへ取得する。

import type { StudioDeps } from "../deps.js";
import { executePlan, localHashes, type SyncItemResult } from "../domain/resource-download.js";
import { resolveInside } from "../domain/resource-sync.js";

export type SyncOutcome =
    | { ok: true; destination: string; transferBytes: number; items: SyncItemResult[] }
    | { ok: false; status: number; reason: string };

export async function syncResources(deps: StudioDeps, code: string, subdir: string,
                                    versionIds: string[]): Promise<SyncOutcome> {
    if (versionIds.length === 0 || versionIds.length > 500) {
        return { ok: false, status: 400, reason: "版は 1〜500 件で指定してください" };
    }
    const projects = await deps.cc.projects();
    if (!projects.ok) return { ok: false, status: 502, reason: `Concordia: ${projects.message}` };
    const project = projects.data.find((p) => p.code === code);
    if (!project) return { ok: false, status: 404, reason: `プロジェクト ${code} は Cc に登録されていません` };
    const dest = resolveInside(project.repo_path, subdir);
    if (!dest) return { ok: false, status: 400, reason: "取得先はプロジェクトの repo の中の相対パスにしてください" };

    const plan = await deps.cu.plan(versionIds, await localHashes(dest));
    if (!plan.ok) return { ok: false, status: plan.status && plan.status < 500 ? plan.status : 502, reason: `Curare: ${plan.message}` };
    // 署名 URL の期限 (300 秒) 内に終える前提。1 件ずつの上限は通常の 8 倍。
    const items = await executePlan(plan.data, dest, deps.cfg.requestTimeoutMs * 8);
    return { ok: true, destination: dest, transferBytes: plan.data.transferBytes, items };
}
