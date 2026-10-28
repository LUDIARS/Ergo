// @implements SPEC-ergo-studio-plugins
/// プラグインの取得。
///   GET    /api/plugins                 台帳 (取得済みパック) と許可された取得元
///   POST   /api/plugins/preview         { source: "owner/repo:subpath[@rev]" } → 中身の確認
///   POST   /api/plugins/install         { token }  承認 → 導入 (次回起動で読み込み)
///   POST   /api/plugins/enable          { id, enabled }
///   DELETE /api/plugins?id=             台帳から外して取得物を消す

import { Hono } from "hono";

import type { StudioDeps } from "../deps.js";
import { parsePluginSource } from "../domain/plugin-source.js";
import { readJson, sendError, str } from "./respond.js";

export function pluginRoutes(deps: StudioDeps): Hono {
    const app = new Hono();

    app.get("/api/plugins", (c) =>
        c.json({ packs: deps.installer.list(), owners: deps.cfg.pluginOwners, home: deps.cfg.home }));

    app.post("/api/plugins/preview", async (c) => {
        const body = await readJson(c);
        const parsed = parsePluginSource(str(body?.source) ?? "", deps.cfg.pluginOwners);
        if (!parsed.ok) return sendError(c, 400, parsed.reason);
        try {
            return c.json({ preview: await deps.installer.preview(parsed.source) });
        } catch (err) {
            return sendError(c, 502, (err as Error).message);
        }
    });

    app.post("/api/plugins/install", async (c) => {
        const body = await readJson(c);
        const token = str(body?.token);
        if (!token) return sendError(c, 400, "token は必須です");
        try {
            return c.json({ pack: await deps.installer.install(token), restartRequired: true }, 201);
        } catch (err) {
            return sendError(c, 409, (err as Error).message);
        }
    });

    app.post("/api/plugins/enable", async (c) => {
        const body = await readJson(c);
        const id = str(body?.id);
        if (!id || typeof body?.enabled !== "boolean") return sendError(c, 400, "id と enabled は必須です");
        const pack = await deps.installer.setEnabled(id, body.enabled);
        return pack ? c.json({ pack, restartRequired: true }) : sendError(c, 404, "パックが見つかりません");
    });

    app.delete("/api/plugins", async (c) => {
        const id = c.req.query("id");
        if (!id) return sendError(c, 400, "id は必須です");
        return (await deps.installer.remove(id)) ? c.json({ removed: id, restartRequired: true })
            : sendError(c, 404, "パックが見つかりません");
    });

    return app;
}
