/// render_pipeline plugin — Phase 3 単一グラフエディタ。
///
/// `spec/tool/render_pipeline_system_b.md` 参照。 Pictor 系統B 解体 (Phase 3
/// `LUDIARS/Pictor#60`) で **ハードコード描画パスが消える** ため、 旧 Scanner
/// モード (Python regex で C++ をスキャンする) は不要になった。 本プラグインは
/// `Pictor/profiles/*.profile.json` を single source of truth とし、 単一の
/// NodeGraph エディタ画面で表示・編集する。
///
/// Protocol:
///   GET  /render_pipeline/api/profiles          → list *.profile.json
///   GET  /render_pipeline/api/profile/:file     → one profile (normalized)
///   POST /render_pipeline/api/profile           → write a profile to disk
///   GET  /render_pipeline/api/health            → profile dir / count + clients
///   WS   /render_pipeline/ws                    → broadcasts {op:"profiles-changed"}
///                                                 on save; relays {op:"timing"}
///                                                 (Phase-2 GPU timestamp) to all
///                                                 connected UI clients

import { Hono } from "hono";
import { WebSocket as WS } from "ws";

import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";
import { normalizeProfile, PROFILE_SCHEMA_VERSION } from "./profile_schema.js";
import {
    listProfiles, loadProfile, saveProfile, resolveProfileDir,
} from "./profile_store.js";

const SCHEMA_VERSION = 2;

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
        description: "Pictor の *.profile.json を単一グラフで編集 (Phase 3)。",
        staticRoot:  "./src/plugins/render_pipeline/ui",

        routes(ctx: PluginContext) {
            const app = new Hono();

            // ───── Profile editor — read/write ─────────────────────────
            app.get("/api/health", (c) => {
                const meta: any = {
                    ok:                     true,
                    version:                SCHEMA_VERSION,
                    clients:                clients.size,
                    profile_schema_version: PROFILE_SCHEMA_VERSION,
                };
                try {
                    const dir = resolveProfileDir();
                    meta.profile_dir   = dir;
                    meta.profile_count = listProfiles(dir).length;
                } catch (e) {
                    meta.profile_dir_err = String(e);
                }
                return c.json(meta);
            });

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
                // Phase 2 GPU timestamp relay (`{op:"timing", frame, passes:[{id,us}]}`)。
                // KS の TimingRelay WS クライアントが送ってくるのをそのまま全 UI へ
                // broadcast。 GraphView の timing オーバーレイがノード着色に使う。
                let msg: any;
                try { msg = JSON.parse(raw.toString()); } catch { return; }
                if (msg?.op === "ping") {
                    ws.send(JSON.stringify({ op: "ack" }));
                } else if (msg?.op === "timing") {
                    broadcast(msg);
                }
            });
            ws.on("close", () => clients.delete(ws));
            ws.on("error", () => {});
        },

        health() {
            let profileDir = "";
            let profileCount = 0;
            try {
                profileDir   = resolveProfileDir();
                profileCount = listProfiles(profileDir).length;
            } catch {}
            return {
                ok:                     true,
                version:                SCHEMA_VERSION,
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
