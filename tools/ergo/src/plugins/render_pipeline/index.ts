/// render_pipeline plugin — two clearly separated halves:
///
///   A) Scanner view ("現状ビュー", read-only)
///      How Pictor's hard-coded Vulkan code (系統B) actually renders today.
///      A Python scanner (`scanner/render_pipeline_scan.py`) statically
///      scans Pictor sources into `render_pipeline.json`; the UI draws a
///      pass DAG + pipeline / shader / attachment tables. Never edited.
///
///   B) Profile editor ("編集ビュー", read/write + disk-persisted)
///      How you *want* Pictor to render (系統A). Reads & writes the
///      canonical `Pictor/profiles/*.profile.json` files which Pictor
///      consumes via `register_presets_from_dir()`. Full CRUD on the
///      `PipelineProfileDef` schema (spec: Pictor/spec/pipeline-profile-config.md).
///
/// The two halves share no state and address different artifacts on disk.
/// The UI surfaces them as two distinct top-level modes so "what Pictor
/// does" and "what Pictor is told to do" are never conflated.
///
/// Protocol:
///   --- Scanner view (A) — read-only ---
///   GET  /render_pipeline/api/snapshot          → scanner JSON snapshot
///   GET  /render_pipeline/api/health            → snapshot stats + profile dir info
///   POST /render_pipeline/api/rescan            → run the Python scanner
///   --- Profile editor (B) — read/write ---
///   GET  /render_pipeline/api/profiles          → list *.profile.json
///   GET  /render_pipeline/api/profile/:file     → one profile (normalized)
///   POST /render_pipeline/api/profile           → write a profile to disk
///   --- WS ---
///   WS   /render_pipeline/ws                    → broadcasts {op:"profiles-changed"}
///                                                 on save + Phase-2 timing relay

import { Hono } from "hono";
import { WebSocket as WS } from "ws";
import { spawn } from "node:child_process";
import { readFileSync, existsSync, statSync } from "node:fs";
import { resolve } from "node:path";

import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";
import { normalizeProfile, PROFILE_SCHEMA_VERSION } from "./profile_schema.js";
import {
    listProfiles, loadProfile, saveProfile, resolveProfileDir,
} from "./profile_store.js";

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

    function broadcast(msg: unknown): void {
        const text = JSON.stringify(msg);
        for (const c of clients) {
            if (c.readyState === WS.OPEN) c.send(text);
        }
    }

    const plugin: Plugin = {
        id:          "render_pipeline",
        title:       "Render Pipeline",
        icon:        "🔲",
        description: "Pictor の render pass を可視化 (scanner) + パイプラインプロファイル *.profile.json を編集。",
        staticRoot:  "./src/plugins/render_pipeline/ui",

        routes(ctx: PluginContext) {
            const app = new Hono();

            // ───── Scanner view (A) — read-only ─────────────────────────
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
                // Profile-editor side health.
                meta.profile_schema_version = PROFILE_SCHEMA_VERSION;
                try {
                    const dir = resolveProfileDir();
                    meta.profile_dir   = dir;
                    meta.profile_count = listProfiles(dir).length;
                } catch (e) {
                    meta.profile_dir_err = String(e);
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

            // ───── Profile editor (B) — read/write ──────────────────────
            // List every *.profile.json the editor can open.
            app.get("/api/profiles", (c) => {
                try {
                    const dir = resolveProfileDir();
                    return c.json({
                        ok:       true,
                        dir,
                        version:  PROFILE_SCHEMA_VERSION,
                        profiles: listProfiles(dir),
                    });
                } catch (e) {
                    return c.json({ ok: false, err: String(e) }, 500);
                }
            });

            // Load one profile, normalized to the full schema.
            app.get("/api/profile/:file", (c) => {
                const file = c.req.param("file");
                try {
                    const r = loadProfile(file);
                    return c.json({ ok: true, ...r });
                } catch (e) {
                    const msg = String((e as Error).message ?? e);
                    const code = /not found/.test(msg) ? 404 : 400;
                    return c.json({ ok: false, err: msg }, code);
                }
            });

            // Write a profile to disk. Body: { profile, file? }.
            // `file` is optional — when absent the canonical
            // <lowercased-profile_name>.profile.json name is derived.
            app.post("/api/profile", async (c) => {
                const body = await c.req.json().catch(() => null);
                if (!body || typeof body !== "object" || !("profile" in body)) {
                    return c.json({ ok: false, err: "body must be { profile, file? }" }, 400);
                }
                try {
                    const profile = normalizeProfile((body as any).profile);
                    const file: string | undefined =
                        typeof (body as any).file === "string" && (body as any).file
                            ? (body as any).file
                            : undefined;
                    const r = saveProfile(profile, file);
                    ctx.log("info", `[render_pipeline] saved profile ${r.file}`);
                    // Tell other clients the on-disk set changed.
                    broadcast({ op: "profiles-changed", file: r.file });
                    return c.json({ ok: true, ...r });
                } catch (e) {
                    return c.json({ ok: false, err: String((e as Error).message ?? e) }, 400);
                }
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
            let profileDir = "";
            let profileCount = 0;
            try {
                profileDir   = resolveProfileDir();
                profileCount = listProfiles(profileDir).length;
            } catch {}
            return {
                ok:                     true,
                version:                SCHEMA_VERSION,
                snapshot_present:       ok,
                clients:                clients.size,
                profile_schema_version: PROFILE_SCHEMA_VERSION,
                profile_dir:            profileDir,
                profile_count:          profileCount,
            };
        },
    };

    return plugin;
};

export default factory;
