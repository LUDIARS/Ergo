// @implements SPEC-ergo-studio-plugins
/// プラグイン取得元の指定と検査 (純関数)。
///
/// 指定: `owner/repo:subpath[@rev]`  (例 `LUDIARS/AdventureCube:tools/ac-web/plugins@main`)
///   - owner は許可リスト (既定 LUDIARS) に入っていること。プラグインは Ergo の
///     プロセス内で動くため、知らない取得元のコードを動かさない。
///   - subpath はリポ内の相対パス。`..`・絶対パス・バックスラッシュは拒否。
///   - rev は省略時 HEAD。取得前に 40 桁の sha へ解決し、台帳にはその sha を固定する。

export interface PluginSource {
    owner:   string;
    repo:    string;
    subpath: string;
    rev:     string;
}

export type ParseResult = { ok: true; source: PluginSource } | { ok: false; reason: string };

const NAME = /^[A-Za-z0-9](?:[A-Za-z0-9._-]{0,98}[A-Za-z0-9_])?$/;
const REV = /^[A-Za-z0-9._/-]{1,200}$/;
export const FULL_SHA = /^[0-9a-f]{40}$/;

export function normalizeSubpath(raw: string): string | null {
    const p = raw.trim().replace(/^\.\/+/, "").replace(/\/+$/, "");
    if (!p || p.includes("\\") || p.startsWith("/") || /^[A-Za-z]:/.test(p)) return null;
    const parts = p.split("/");
    if (parts.some((s) => s === "" || s === "." || s === ".." || !/^[A-Za-z0-9._-]+$/.test(s))) return null;
    return parts.join("/");
}

export function parsePluginSource(spec: string, allowedOwners: string[]): ParseResult {
    const m = spec.trim().match(/^([^/\s:@]+)\/([^/\s:@]+):([^@\s]+)(?:@(\S+))?$/);
    if (!m) return { ok: false, reason: "`owner/repo:subpath[@rev]` の形で指定してください" };
    const [, owner, repo, rawSub, rawRev] = m;
    if (!NAME.test(owner!) || !NAME.test(repo!)) return { ok: false, reason: "owner / repo 名が不正です" };
    if (!allowedOwners.some((o) => o.toLowerCase() === owner!.toLowerCase())) {
        return { ok: false, reason: `取得元 ${owner} は許可リストにありません (ERGO_PLUGIN_OWNERS)` };
    }
    const subpath = normalizeSubpath(rawSub!);
    if (!subpath) return { ok: false, reason: "subpath が不正です (リポ内の相対パス)" };
    const rev = rawRev ?? "HEAD";
    if (!REV.test(rev) || rev.includes("..")) return { ok: false, reason: "rev が不正です" };
    return { ok: true, source: { owner: owner!, repo: repo!, subpath, rev } };
}

export function packId(s: Pick<PluginSource, "owner" | "repo" | "subpath">): string {
    return `${s.owner}/${s.repo}:${s.subpath}`;
}
