/// Profile Timeline plugin — ergo_profile が出力した Chrome Trace Event JSON を
/// browser でタイムライン可視化する Ergo plugin。
///
/// アーキ:
///   - サーバ側はほぼ何もしない (トレースはクライアント側 FileReader で読む)。
///   - WS 経由で「直近に publish されたトレース」を他クライアントへ broadcast
///     する軽い hub にしておく (将来 engine 側からのライブ送信用)。
///   - UI 本体は `ui/` (canvas 描画のフレームグラフ + カウンタトラック)。

import { Hono } from "hono";
import { WebSocket as WS } from "ws";

import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";

const SCHEMA_VERSION = 1;

type Inbound =
    | { op: "publish"; trace: string }
    | { op: "ping" };

type Outbound =
    | { op: "trace"; trace: string | null; clients: number }
    | { op: "ack" };

const factory: PluginFactory = () => {
    let current: string | null = null; // 直近 publish された trace JSON
    const clients = new Set<WS>();

    function send(ws: WS, msg: Outbound): void {
        if (ws.readyState === WS.OPEN) ws.send(JSON.stringify(msg));
    }
    function broadcast(msg: Outbound): void {
        const text = JSON.stringify(msg);
        for (const c of clients) if (c.readyState === WS.OPEN) c.send(text);
    }

    const plugin: Plugin = {
        id:          "profile",
        title:       "Profile Timeline",
        icon:        "⏱",
        description:
            "Drop an ergo_profile Chrome trace (.json) to inspect the performance timeline.",
        staticRoot:  "./src/plugins/profile/ui",

        routes(_ctx: PluginContext) {
            const app = new Hono();
            app.get("/api/health", (c) =>
                c.json({ ok: true, version: SCHEMA_VERSION, clients: clients.size })
            );
            return app;
        },

        onUpgrade(_req, ws: WS, _ctx) {
            clients.add(ws);
            send(ws, { op: "trace", trace: current, clients: clients.size });

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
                        if (typeof msg.trace === "string") {
                            current = msg.trace;
                            broadcast({ op: "trace", trace: current, clients: clients.size });
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
                has_trace: current !== null,
            };
        },
    };

    return plugin;
};

export default factory;
