// @implements SPEC-ergo-studio-sources
/// 各プロジェクトのソース (git) の状態と同期。
///
/// 同期は「取得 (fetch)」と「main の早送り (clean な main のときだけ)」に限る。
/// main への反映は Revisor の local PR → 審査 → マージが担い、ここからは push しない。
/// 共有の作業ツリーを他セッションが使っていることがあるため、未コミットの変更や
/// 別ブランチにいる checkout は動かさない (domain/source-sync.ts が判定する)。

import { runFile } from "../net/exec.js";
import { parseBranchStatus, type BranchStatus } from "../domain/source-sync.js";

export interface LastCommit {
    sha:     string;
    subject: string;
    date:    string;
}

export interface SourceStatus extends BranchStatus {
    repoPath:   string;
    lastCommit: LastCommit | null;
}

export class GitClient {
    constructor(private readonly timeoutMs: number) {}

    async status(repoPath: string): Promise<{ ok: true; data: SourceStatus } | { ok: false; message: string }> {
        const st = await runFile("git", ["-C", repoPath, "status", "--porcelain=v2", "--branch"], { timeoutMs: this.timeoutMs });
        if (!st.ok) return { ok: false, message: st.spawnError ?? (st.stderr.trim() || "git status に失敗しました") };
        const log = await runFile("git", ["-C", repoPath, "log", "-1", "--format=%h%x00%s%x00%cI"], { timeoutMs: this.timeoutMs });
        let lastCommit: LastCommit | null = null;
        if (log.ok) {
            const [sha, subject, date] = log.stdout.trim().split("\u0000");
            if (sha) lastCommit = { sha, subject: subject ?? "", date: date ?? "" };
        }
        return { ok: true, data: { repoPath, ...parseBranchStatus(st.stdout), lastCommit } };
    }

    async fetch(repoPath: string): Promise<{ ok: boolean; message: string }> {
        const r = await runFile("git", ["-C", repoPath, "fetch", "--prune", "origin"], { timeoutMs: this.timeoutMs * 8 });
        return { ok: r.ok, message: r.ok ? "取得しました" : (r.spawnError ?? r.stderr.trim()) };
    }

    async fastForward(repoPath: string, branch: string): Promise<{ ok: boolean; message: string }> {
        const r = await runFile("git", ["-C", repoPath, "merge", "--ff-only", `origin/${branch}`], { timeoutMs: this.timeoutMs * 4 });
        return { ok: r.ok, message: r.ok ? "早送りしました" : (r.spawnError ?? r.stderr.trim()) };
    }
}
