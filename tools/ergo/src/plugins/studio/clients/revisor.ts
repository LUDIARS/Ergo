// @implements SPEC-ergo-studio-sources
/// Revisor — local PR の一覧 (審査・マージの状態)。
/// Revisor の CLI (`<LUDIARS_ROOT>/Revisor/src/cli.mjs pr list --json`) を読み取りだけで使う。
/// PR の提出・マージはここから行わない (Cc の作業フローと人間の判断が持つ)。

import { existsSync } from "node:fs";
import { join } from "node:path";

import { runFile } from "../net/exec.js";

export interface RevisorPr {
    number:      number;
    repository:  string;
    title:       string;
    headRef:     string;
    baseRef:     string;
    status:      string;
    checkStatus: string | null;
    updatedAt?:  string;
    createdAt?:  string;
}

export class RevisorClient {
    constructor(private readonly ludiarsRoot: string | null, private readonly timeoutMs: number) {}

    private cli(): string | null {
        if (!this.ludiarsRoot) return null;
        const p = join(this.ludiarsRoot, "Revisor", "src", "cli.mjs");
        return existsSync(p) ? p : null;
    }

    get available(): boolean {
        return this.cli() !== null;
    }

    /// 疎通確認 (登録リポ一覧を引けるか)。PR 一覧より軽い。
    async ping(): Promise<{ ok: true } | { ok: false; message: string }> {
        const cli = this.cli();
        if (!cli) return { ok: false, message: "Revisor の CLI が見つかりません (LUDIARS_ROOT 未設定か未クローン)" };
        const r = await runFile(process.execPath, [cli, "repo", "list", "--json"], { timeoutMs: this.timeoutMs * 2 });
        return r.ok ? { ok: true } : { ok: false, message: r.spawnError ?? (r.stderr.trim().split("\n").pop() ?? "失敗") };
    }

    async prs(repository: string | null): Promise<{ ok: true; data: RevisorPr[] } | { ok: false; message: string }> {
        const cli = this.cli();
        if (!cli) return { ok: false, message: "Revisor の CLI が見つかりません (LUDIARS_ROOT 未設定か未クローン)" };
        const args = [cli, "pr", "list", "--json"];
        if (repository) args.push("--repository", repository);
        const r = await runFile(process.execPath, args, { timeoutMs: this.timeoutMs * 2 });
        if (!r.ok) return { ok: false, message: r.spawnError ?? (r.stderr.trim().split("\n").pop() ?? "Revisor 呼び出しに失敗") };
        try {
            const list = JSON.parse(r.stdout) as Array<Record<string, unknown>>;
            return {
                ok: true,
                data: list.map((p) => ({
                    number:      Number(p.number),
                    repository:  String(p.repository ?? ""),
                    title:       String(p.title ?? ""),
                    headRef:     String(p.headRef ?? ""),
                    baseRef:     String(p.baseRef ?? ""),
                    status:      String(p.status ?? ""),
                    checkStatus: typeof p.checkStatus === "string" ? p.checkStatus : null,
                    updatedAt:   typeof p.updatedAt === "string" ? p.updatedAt : undefined,
                    createdAt:   typeof p.createdAt === "string" ? p.createdAt : undefined,
                })),
            };
        } catch {
            return { ok: false, message: "Revisor の応答が JSON ではありません" };
        }
    }
}
