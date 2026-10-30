// @implements SPEC-ergo-studio-resources
/// Curare の版を手元へ取得するときの判定 (純関数)。
///
/// Curare の取得計画は `overwrite: false` (取得先の保護)。手元に同名で別内容の
/// ファイルがあれば上書きせず「衝突」として残し、人に選ばせる。

import { isAbsolute, normalize, relative, resolve, sep } from "node:path";

export type SyncAction =
    | { action: "skip_same" }                   // 同じ内容が既にある
    | { action: "download" }                    // 無いので取得する
    | { action: "conflict"; localHash: string } // 別の内容がある (上書きしない)
    | { action: "unavailable" };                // 取得が要るのに URL が無い

export function decideSyncAction(item: { contentHash: string; needsDownload: boolean; downloadUrl: string | null },
                                 localHash: string | null): SyncAction {
    if (localHash === item.contentHash) return { action: "skip_same" };
    if (localHash !== null) return { action: "conflict", localHash };
    if (!item.downloadUrl) return item.needsDownload ? { action: "unavailable" } : { action: "skip_same" };
    return { action: "download" };
}

const RESERVED = /^(con|prn|aux|nul|com\d|lpt\d)(\..*)?$/i;

/// Curare のリソース名を手元のファイル名にする。区切り文字や `..` を使わせず、
/// Windows の予約名・制御文字を避ける。使えなければ null。
export function safeFileName(name: string): string | null {
    const base = name.replace(/[\\/]/g, "_").replace(/[\u0000-\u001f<>:"|?*]/g, "_").trim();
    if (!base || base === "." || base === ".." || RESERVED.test(base)) return null;
    return base.length > 200 ? base.slice(0, 200) : base;
}

/// 取得先をプロジェクトの repo の中に限る。repo 外・絶対パス指定は null。
export function resolveInside(repoPath: string, subdir: string): string | null {
    if (isAbsolute(subdir)) return null;
    const target = resolve(repoPath, normalize(subdir || "."));
    const rel = relative(resolve(repoPath), target);
    if (rel === "" || (!rel.startsWith("..") && !isAbsolute(rel) && !rel.split(sep).includes(".."))) return target;
    return null;
}

/// Curare の内容ハッシュ書式 (`sha256:<64 hex>`)。
export function toContentHash(hexDigest: string): string {
    return `sha256:${hexDigest.toLowerCase()}`;
}
