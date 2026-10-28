// @implements SPEC-ergo-studio-plugins
/// プラグインパックの取得 (プレビュー → 承認 → 導入) と台帳の更新。
///
///  1. preview: rev を 40 桁の sha に解決し、その sha の subpath だけを一時領域へ取得して
///     中のプラグイン (index.js / index.ts を持つサブディレクトリ) を列挙する。
///  2. install: 承認されたプレビューの一時領域を、そのまま `<home>/plugins/packs/...` へ移す。
///     見せた内容と入る内容がずれない (再取得しない)。依存は `npm install --ignore-scripts`
///     (パッケージのスクリプトを走らせない)。台帳には sha を固定して記録する。
///  3. 読み込みは次回起動時 (core/external.ts)。実行中のサーバへは差し込まない。

import { randomUUID } from "node:crypto";
import { existsSync } from "node:fs";
import { mkdir, readdir, rename, rm } from "node:fs/promises";
import { dirname, join } from "node:path";

import { readLedger, writeLedger, type LedgerPack } from "../../../core/plugin-ledger.js";
import { runFile } from "../net/exec.js";
import { FULL_SHA, packId, type PluginSource } from "./plugin-source.js";

export interface PluginPreview {
    token:   string;
    id:      string;
    owner:   string;
    repo:    string;
    subpath: string;
    rev:     string;      // 解決済みの sha
    requested: string;    // 指定された rev
    commit:  { subject: string; date: string };
    plugins: string[];
    hasDependencies: boolean;
    replaces: string | null;   // 同じ id の既存パックの rev
    expiresAt: string;
}

interface Staged {
    preview: PluginPreview;
    dir:     string;
    at:      number;
}

const PREVIEW_TTL_MS = 10 * 60 * 1000;

export class PluginInstaller {
    private readonly staged = new Map<string, Staged>();

    constructor(private readonly home: string, private readonly timeoutMs: number) {}

    private repoUrl(s: PluginSource): string {
        return `https://github.com/${s.owner}/${s.repo}.git`;
    }

    private async resolveRev(s: PluginSource): Promise<string> {
        if (FULL_SHA.test(s.rev)) return s.rev;
        const r = await runFile("git", ["ls-remote", this.repoUrl(s), s.rev], { timeoutMs: this.timeoutMs * 4 });
        if (!r.ok) throw new Error(r.spawnError ?? `rev を解決できません: ${r.stderr.trim()}`);
        const line = r.stdout.split(/\r?\n/).find((l) => l.trim());
        const sha = line?.split(/\s+/)[0];
        if (!sha || !FULL_SHA.test(sha)) throw new Error(`rev ${s.rev} が見つかりません`);
        return sha;
    }

    private async git(args: string[], cwd: string): Promise<string> {
        const r = await runFile("git", args, { cwd, timeoutMs: this.timeoutMs * 8 });
        if (!r.ok) throw new Error(r.spawnError ?? (r.stderr.trim() || `git ${args[0]} に失敗`));
        return r.stdout;
    }

    private sweep(): void {
        const now = Date.now();
        for (const [token, s] of this.staged) {
            if (now - s.at > PREVIEW_TTL_MS) {
                this.staged.delete(token);
                void rm(s.dir, { recursive: true, force: true });
            }
        }
    }

    async preview(source: PluginSource): Promise<PluginPreview> {
        this.sweep();
        const sha = await this.resolveRev(source);
        const dir = join(this.home, "plugins", "staging", randomUUID());
        await mkdir(dir, { recursive: true });
        try {
            await this.git(["init", "-q"], dir);
            await this.git(["remote", "add", "origin", this.repoUrl(source)], dir);
            await this.git(["sparse-checkout", "set", "--no-cone", `/${source.subpath}/`], dir);
            await this.git(["fetch", "-q", "--depth", "1", "origin", sha], dir);
            await this.git(["checkout", "-q", "FETCH_HEAD"], dir);
            const log = await this.git(["log", "-1", "--format=%s%x00%cI"], dir);
            const [subject = "", date = ""] = log.trim().split("\u0000");
            const packDir = join(dir, source.subpath);
            if (!existsSync(packDir)) throw new Error(`${source.subpath} はこのリビジョンにありません`);
            const plugins: string[] = [];
            for (const ent of await readdir(packDir, { withFileTypes: true })) {
                if (!ent.isDirectory()) continue;
                if (["index.js", "index.ts"].some((f) => existsSync(join(packDir, ent.name, f)))) plugins.push(ent.name);
            }
            if (plugins.length === 0) throw new Error("プラグイン (index.js / index.ts を持つサブディレクトリ) がありません");
            const id = packId(source);
            const existing = readLedger(this.home).packs.find((p) => p.id === id);
            const token = randomUUID();
            const preview: PluginPreview = {
                token, id, owner: source.owner, repo: source.repo, subpath: source.subpath,
                rev: sha, requested: source.rev, commit: { subject, date }, plugins: plugins.sort(),
                hasDependencies: existsSync(join(packDir, "package.json")),
                replaces: existing ? existing.rev : null,
                expiresAt: new Date(Date.now() + PREVIEW_TTL_MS).toISOString(),
            };
            this.staged.set(token, { preview, dir, at: Date.now() });
            return preview;
        } catch (err) {
            await rm(dir, { recursive: true, force: true });
            throw err;
        }
    }

    async install(token: string): Promise<LedgerPack> {
        this.sweep();
        const staged = this.staged.get(token);
        if (!staged) throw new Error("プレビューが見つからないか期限切れです。もう一度プレビューしてください");
        this.staged.delete(token);
        const p = staged.preview;
        // 取得は subpath だけの sparse checkout なので、subpath ごとに置き場を分ける。
        const checkoutDir = join(this.home, "plugins", "packs", p.owner, p.repo, p.rev, p.subpath.replace(/\//g, "__"));
        try {
            if (!existsSync(checkoutDir)) {
                await mkdir(dirname(checkoutDir), { recursive: true });
                await rename(staged.dir, checkoutDir);
            } else {
                await rm(staged.dir, { recursive: true, force: true });
            }
            const packDir = join(checkoutDir, p.subpath);
            if (existsSync(join(packDir, "package.json"))) {
                const r = await runFile("npm", ["install", "--ignore-scripts", "--omit=dev", "--no-audit", "--no-fund"],
                    { cwd: packDir, timeoutMs: this.timeoutMs * 30 });
                if (!r.ok) throw new Error(r.spawnError ?? `依存の導入に失敗: ${r.stderr.trim().split("\n").pop()}`);
            }
        } catch (err) {
            await rm(staged.dir, { recursive: true, force: true });
            throw err;
        }
        const entry: LedgerPack = {
            id: p.id, owner: p.owner, repo: p.repo, subpath: p.subpath, rev: p.rev,
            checkoutDir, enabled: true, installedAt: new Date().toISOString(), plugins: p.plugins,
        };
        const ledger = readLedger(this.home);
        ledger.packs = [...ledger.packs.filter((x) => x.id !== p.id), entry];
        await writeLedger(this.home, ledger);
        return entry;
    }

    async setEnabled(id: string, enabled: boolean): Promise<LedgerPack | null> {
        const ledger = readLedger(this.home);
        const pack = ledger.packs.find((p) => p.id === id);
        if (!pack) return null;
        pack.enabled = enabled;
        await writeLedger(this.home, ledger);
        return pack;
    }

    /// 台帳から外す。取得物は他の id が同じ checkout を使っていなければ消す。
    async remove(id: string): Promise<boolean> {
        const ledger = readLedger(this.home);
        const pack = ledger.packs.find((p) => p.id === id);
        if (!pack) return false;
        ledger.packs = ledger.packs.filter((p) => p.id !== id);
        await writeLedger(this.home, ledger);
        if (!ledger.packs.some((p) => p.checkoutDir === pack.checkoutDir)) {
            await rm(pack.checkoutDir, { recursive: true, force: true });
        }
        return true;
    }

    list(): LedgerPack[] {
        return readLedger(this.home).packs;
    }
}
