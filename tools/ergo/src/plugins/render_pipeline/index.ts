/// render_pipeline plugin — Pictor の render pass DAG / pipeline spec /
/// shader / attachment flow を Web で可視化する。 ハイブリッド構成:
///
///   - 静的 scan: `scanner/render_pipeline_scan.py` が事前に出力した
///     `render_pipeline.json` を /api/snapshot で配信
///   - 将来の実行時 GPU timestamp: Pictor 側が WS で `{op:"timing"}` を
///     publish する想定。 サーバ側で受け取って web に転送する hub だけ
///     先に用意 (実装は Phase 2)
///
/// プロトコル (Phase 1 は最小):
///   GET  /render_pipeline/api/snapshot   → JSON snapshot
///   GET  /render_pipeline/api/health     → { ok, snapshot_present, shaders }
///   POST /render_pipeline/api/rescan     → scanner を子プロセスで走らせる
///   WS   /render_pipeline/ws             → Phase 2 用 (現在は ack だけ返す)
///
/// snapshot ファイルの場所:
///   tools/ergo/scanner/render_pipeline.json
///
/// 静的サイト化 (GitHub Pages 等) では snapshot.json と shader 内容が
/// 既に JSON に embed されているので、 サーバを介さず UI だけでも動く
/// (api/snapshot を fetch する fallback として `./snapshot.json` も試す)。

import { Hono } from "hono";
import { WebSocket as WS } from "ws";
import { spawn } from "node:child_process";
import { readFileSync, existsSync, statSync } from "node:fs";
import { resolve } from "node:path";

import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";

const SCHEMA_VERSION = 1;

function snapshotPath(): string {
    return resolve(process.cwd(), "scanner", "render_pipeline.json");
}

function loadSnapshot(): any {
    const p = snapshotPath();
    if (!existsSync(p)) return null;
    try {
        return JSON.parse(readFileSync(p, "utf-8"));
    } catch {
        return null;
    }
}

const factory: PluginFactory = () => {
    const clients = new Set<WS>();

    const plugin: Plugin = {
        id:          "render_pipeline",
        title:       "Render Pipeline",
        icon:        "🔲",
        description: "Pictor の render pass DAG / pipeline spec / shader / attachment flow を静的 scan ベースで可視化。",
        staticRoot:  "./src/plugins/render_pipeline/ui",

        routes(ctx: PluginContext) {
            const app = new Hono();

            app.get("/api/snapshot", (c) => {
                const snap = loadSnapshot();
                if (!snap) return c.json({ ok: false, err: "snapshot not found — run scanner/render_pipeline_scan.py" }, 404);
                return c.json(snap);
            });

            app.get("/api/health", (c) => {
                const p = snapshotPath();
                const ok = existsSync(p);
                const meta: any = { ok, version: SCHEMA_VERSION, clients: clients.size };
                if (ok) {
                    try {
                        const st = statSync(p);
                        meta.snapshot_bytes = st.size;
                        meta.snapshot_mtime = st.mtime.toISOString();
                    } catch {}
                    const snap = loadSnapshot();
                    if (snap) {
                        meta.passes      = snap.passes?.length ?? 0;
                        meta.pipelines   = snap.pipelines?.length ?? 0;
                        meta.shaders     = snap.shaders?.length ?? 0;
                        meta.attachments = snap.attachments?.length ?? 0;
                        meta.scanned_at  = snap.scanned_at;
                    }
                }
                return c.json(meta);
            });

            app.post("/api/rescan", async (c) => {
                // scanner を別 process で起動。 完了を待ってから snapshot を返す。
                const script = resolve(process.cwd(), "scanner", "render_pipeline_scan.py");
                if (!existsSync(script)) {
                    return c.json({ ok: false, err: `scanner not found: ${script}` }, 500);
                }
                interface ScanResult { ok: boolean; exit?: number | null; stdout: string; stderr?: string; err?: string; snapshot?: unknown; }
                const result = await new Promise<ScanResult>((res) => {
                    const proc = spawn("python", [script], { cwd: resolve(process.cwd()) });
                    let out = "";
                    let err = "";
                    proc.stdout.on("data", (b) => { out += b.toString(); });
                    proc.stderr.on("data", (b) => { err += b.toString(); });
                    proc.on("close", (code) => {
                        if (code !== 0) {
                            ctx.log("warn", `[render_pipeline] rescan exit=${code} stderr=${err.trim()}`);
                            res({ ok: false, exit: code, stdout: out, stderr: err });
                            return;
                        }
                        ctx.log("info", `[render_pipeline] rescan ok: ${out.trim()}`);
                        res({ ok: true, stdout: out, snapshot: loadSnapshot() });
                    });
                    proc.on("error", (e) => {
                        ctx.log("warn", `[render_pipeline] rescan spawn 失敗: ${e}`);
                        res({ ok: false, stdout: "", err: String(e) });
                    });
                });
                return c.json(result, result.ok ? 200 : 500);
            });

            return app;
        },

        onUpgrade(_req, ws: WS, _ctx) {
            clients.add(ws);
            ws.on("message", (raw: any) => {
                // Phase 2 で {op:"timing", pass:"scene_hdr", us:1234} を受け取って relay。
                // Phase 1 は ping だけ ack 返す。
                let msg: any;
                try { msg = JSON.parse(raw.toString()); } catch { return; }
                if (msg?.op === "ping") ws.send(JSON.stringify({ op: "ack" }));
            });
            ws.on("close", () => clients.delete(ws));
            ws.on("error", () => {});
        },

        health() {
            const p = snapshotPath();
            const ok = existsSync(p);
            return {
                ok:                 true,
                version:            SCHEMA_VERSION,
                snapshot_present:   ok,
                clients:            clients.size,
            };
        },
    };

    return plugin;
};

export default factory;
