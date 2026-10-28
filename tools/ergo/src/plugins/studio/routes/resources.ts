// @implements SPEC-ergo-studio-resources
/// リソース (Curare)。
///   GET  /api/resources/projects                         Cu のプロジェクト
///   GET  /api/resources/projects/:id/folders             フォルダ
///   GET  /api/resources/projects/:id/folders/:fid/assets アセット
///   GET  /api/resources/assets/:assetId/versions         版
///   POST /api/resources/sync  { code, subdir, versionIds }  Cc プロジェクトの repo 内へ取得

import { Hono } from "hono";

import type { StudioDeps } from "../deps.js";
import { syncResources } from "../usecases/sync-resources.js";
import { readJson, sendError, sendResult, str } from "./respond.js";

export function resourceRoutes(deps: StudioDeps): Hono {
    const app = new Hono();

    app.get("/api/resources/projects", async (c) =>
        sendResult(c, "curare", await deps.cu.projects(), (projects) => ({ projects })));
    app.get("/api/resources/projects/:id/folders", async (c) =>
        sendResult(c, "curare", await deps.cu.folders(c.req.param("id")), (folders) => ({ folders })));
    app.get("/api/resources/projects/:id/folders/:fid/assets", async (c) =>
        sendResult(c, "curare", await deps.cu.assets(c.req.param("id"), c.req.param("fid")), (assets) => ({ assets })));
    app.get("/api/resources/assets/:assetId/versions", async (c) =>
        sendResult(c, "curare", await deps.cu.versions(c.req.param("assetId")), (versions) => ({ versions })));

    app.post("/api/resources/sync", async (c) => {
        const body = await readJson(c);
        const code = str(body?.code)?.trim();
        const subdir = str(body?.subdir) ?? "";
        const ids = Array.isArray(body?.versionIds) ? body!.versionIds.filter((v): v is string => typeof v === "string") : [];
        if (!code) return sendError(c, 400, "code は必須です");
        const out = await syncResources(deps, code, subdir, ids);
        return out.ok ? c.json(out) : c.json({ error: "sync", message: out.reason }, out.status as 400);
    });

    return app;
}
