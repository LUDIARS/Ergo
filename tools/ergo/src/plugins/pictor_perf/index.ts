/// Pictor Perf Monitor plugin — Pictor の PerfQueryAPI::export_json() が吐く
/// 性能スナップショットを browser で可視化する Ergo plugin。
///
/// 可視化内容 (Pictor spec/subsystem/perf_introspection.md):
///   - A 構造的: SoA stream 毎のアライメント / キャッシュライン跨ぎ / ライン利用率、
///     アロケータ断片化、DoD 不変条件 (float4x4==64 / AABB==24)。
///   - B モデル推定: パス毎の実効帯域 (GB/s)。
///   - バッチ適格性: per-object→per-batch の畳み込み可否と理由 (GPU バッチ対象か)。
///   - per-batch GPU 時間 / C HW カウンタ (L2/L3 ヒット率・DRAM 帯域)。
///
/// アーキ (profile plugin と同型):
///   - host (Pictor を回すゲーム / Ergo render context) が export_json() を
///     定期的に publish する。経路は 2 つ:
///       * WS: { op:"publish", snapshot:"<json string>" }
///       * HTTP: POST /pictor_perf/api/publish  (body = perf JSON、headless 用)
///   - plugin は直近スナップショットを保持し、接続中の全 UI へ broadcast する。

import { Hono } from "hono";
import { WebSocket as WS } from "ws";

import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";

const SCHEMA_VERSION = 1;

type Inbound =
    | { op: "publish"; snapshot: string }
    | { op: "ping" };

type Outbound =
    | { op: "snapshot"; snapshot: string | null; clients: number }
    | { op: "ack" };

const factory: PluginFactory = () => {
    let current: string | null = null; // 直近 publish された perf JSON (生文字列)
    const clients = new Set<WS>();

    function send(ws: WS, msg: Outbound): void {
        if (ws.readyState === WS.OPEN) ws.send(JSON.stringify(msg));
    }
    function broadcast(msg: Outbound): void {
        const text = JSON.stringify(msg);
        for (const c of clients) if (c.readyState === WS.OPEN) c.send(text);
    }

    /// publish された文字列が妥当な perf スナップショットか軽く検証する
    /// (壊れた payload を保持して UI を壊さないため。無言で握り潰さず false を返す)。
    function accept(snapshot: string): boolean {
        try {
            const o = JSON.parse(snapshot);
            return !!o && typeof o === "object" && "memory" in o;
        } catch {
            return false;
        }
    }

    const plugin: Plugin = {
        id:          "pictor_perf",
        title:       "Pictor Perf Monitor",
        icon:        "📊",
        description:
            "Pictor の per-object/batch 性能・DoD キャッシュ効率・GPU バッチ適格性を可視化。",
        staticRoot:  "./src/plugins/pictor_perf/ui",

        routes(ctx: PluginContext) {
            const app = new Hono();
            app.get("/api/health", (c) =>
                c.json({ ok: true, version: SCHEMA_VERSION, clients: clients.size, has_snapshot: current !== null })
            );
            // headless host 用の取り込み口。body をそのまま perf JSON として受ける。
            app.post("/api/publish", async (c) => {
                const text = await c.req.text();
                if (!accept(text)) {
                    ctx.log("warn", "pictor_perf: rejected invalid perf snapshot");
                    return c.json({ ok: false, error: "invalid perf snapshot (memory フィールド必須)" }, 400);
                }
                current = text;
                broadcast({ op: "snapshot", snapshot: current, clients: clients.size });
                return c.json({ ok: true, clients: clients.size });
            });
            // 直近スナップショットの素読み出し (UI 初期化 / 単発取得用)。
            app.get("/api/snapshot", (c) => {
                if (current === null) return c.json({ ok: false, error: "no snapshot yet" }, 404);
                return c.body(current, 200, { "content-type": "application/json" });
            });
            return app;
        },

        onUpgrade(_req, ws: WS, ctx: PluginContext) {
            clients.add(ws);
            send(ws, { op: "snapshot", snapshot: current, clients: clients.size });

            ws.on("message", (raw: unknown) => {
                let msg: Inbound;
                try {
                    msg = JSON.parse(String(raw));
                } catch {
                    return;
                }
                if (!msg || typeof msg !== "object" || !("op" in msg)) return;
                switch (msg.op) {
                    case "publish":
                        if (typeof msg.snapshot === "string" && accept(msg.snapshot)) {
                            current = msg.snapshot;
                            broadcast({ op: "snapshot", snapshot: current, clients: clients.size });
                        } else {
                            ctx.log("warn", "pictor_perf: WS publish rejected (invalid snapshot)");
                        }
                        break;
                    case "ping":
                        send(ws, { op: "ack" });
                        break;
                }
            });

            ws.on("close", () => { clients.delete(ws); });
            ws.on("error", () => { /* close handler tidies up */ });
        },

        health() {
            return {
                ok: true,
                version: SCHEMA_VERSION,
                clients: clients.size,
                has_snapshot: current !== null,
            };
        },
    };

    return plugin;
};

export default factory;
