// @implements SPEC-ergo-studio-connections
/// 連携の可視化。
///   GET /api/connections   各サービスの確認結果と連携図 (ノード・辺)、topology の他サービス
///   GET /api/endpoints     解決済みの接続先 (Studio が使うもの)

import { Hono } from "hono";

import type { StudioDeps } from "../deps.js";
import { ConnectionProber } from "../usecases/probe-connections.js";

export function connectionRoutes(deps: StudioDeps): Hono {
    const app = new Hono();
    const prober = new ConnectionProber(deps);

    app.get("/api/connections", async (c) => c.json(await prober.probe()));

    app.get("/api/endpoints", async (c) => {
        const snap = await deps.endpoints.snapshot();
        return c.json({ resolvedAt: snap.resolvedAt, topology: snap.topology, urls: snap.urls });
    });

    return app;
}
