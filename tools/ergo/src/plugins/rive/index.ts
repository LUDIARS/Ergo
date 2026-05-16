/// Rive Player plugin — Rive ファイル (.riv) を browser canvas で再生し、
/// アートボード / state machine / animation timeline を可視化する Ergo plugin。
///
/// アーキ:
///   - サーバ側はほぼ何もしない (.riv はクライアント側 FileReader で読む)。
///   - WS 経由で「現在開いてる .riv のメタ情報」を他クライアントに broadcast
///     できる軽い hub にしておく (将来 KS など engine 側と繋ぐ用途)。
///   - UI 本体は `ui/index.html` (Rive WebGL via @rive-app/canvas-advanced)。

import { Hono } from "hono";
import { WebSocket as WS } from "ws";

import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";

const SCHEMA_VERSION = 1;

interface RivMeta {
    name: string;             // ファイル名
    size: number;             // bytes
    artboards: ArtboardMeta[];
}

interface ArtboardMeta {
    name:                string;
    width:               number;
    height:              number;
    stateMachineNames:   string[];
    animationNames:      string[];
    animationDurations:  number[];  // 秒
}

type Inbound =
    | { op: "publish"; meta: RivMeta }
    | { op: "ping" };

type Outbound =
    | { op: "meta"; meta: RivMeta | null; clients: number }
    | { op: "ack" };

const factory: PluginFactory = () => {
    let current: RivMeta | null = null;
    const clients = new Set<WS>();

    function send(ws: WS, msg: Outbound) {
        if (ws.readyState === WS.OPEN) ws.send(JSON.stringify(msg));
    }
    function broadcast(msg: Outbound) {
        const text = JSON.stringify(msg);
        for (const c of clients) if (c.readyState === WS.OPEN) c.send(text);
    }

    const plugin: Plugin = {
        id:          "rive",
        title:       "Rive Player",
        icon:        "▶",
        description:
            "Drop a .riv file to play it. Inspect artboards, state machines, animations and timeline.",
        staticRoot:  "./src/plugins/rive/ui",

        routes(_ctx: PluginContext) {
            const app = new Hono();
            app.get("/api/meta", (c) => c.json({ meta: current, clients: clients.size }));
            app.get("/api/health", (c) =>
                c.json({ ok: true, version: SCHEMA_VERSION, clients: clients.size })
            );
            return app;
        },

        onUpgrade(_req, ws: WS, _ctx) {
            clients.add(ws);
            // 接続直後に現在の meta を流す。 null なら null をそのまま送る (= 未選択)。
            send(ws, { op: "meta", meta: current, clients: clients.size });
            // 他のクライアントには接続者数だけ更新
            broadcast({ op: "meta", meta: current, clients: clients.size });

            ws.on("message", (raw: any) => {
                let msg: Inbound;
                try {
                    msg = JSON.parse(raw.toString());
                } catch {
                    return;
                }
                if (!msg || typeof msg !== "object" || !("op" in msg)) return;
                switch (msg.op) {
                    case "publish":
                        if (msg.meta && typeof msg.meta === "object") {
                            current = msg.meta;
                            broadcast({ op: "meta", meta: current, clients: clients.size });
                        }
                        break;
                    case "ping":
                        send(ws, { op: "ack" });
                        break;
                }
            });

            ws.on("close", () => {
                clients.delete(ws);
                broadcast({ op: "meta", meta: current, clients: clients.size });
            });
            ws.on("error", () => { /* close handler tidies up */ });
        },

        health() {
            return {
                ok: true,
                version: SCHEMA_VERSION,
                clients: clients.size,
                has_meta: current !== null,
            };
        },
    };

    return plugin;
};

export default factory;
