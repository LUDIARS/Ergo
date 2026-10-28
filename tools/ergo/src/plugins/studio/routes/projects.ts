// @implements SPEC-ergo-studio-instruct
/// プロジェクト (Cc 登録) と AI への指示。
///   GET  /api/projects                  登録一覧
///   POST /api/projects                  登録 { code, repo_path, project? }
///   GET  /api/templates                 有効な委託テンプレート
///   POST /api/projects/:code/instruct   指示 { call_name, args, extra_prompt? }
///   GET  /api/runs                      Studio から出した委託 (triggered_by=ergo-studio)
///   GET  /api/runs/:id                  委託 1 件

import { Hono } from "hono";

import type { StudioDeps } from "../deps.js";
import { runBelongsTo } from "../domain/delegation-request.js";
import { instruct } from "../usecases/instruct.js";
import { readJson, sendError, sendResult, str } from "./respond.js";

export function projectRoutes(deps: StudioDeps): Hono {
    const app = new Hono();

    app.get("/api/projects", async (c) => sendResult(c, "concordia", await deps.cc.projects(), (projects) => ({ projects })));

    app.post("/api/projects", async (c) => {
        const body = await readJson(c);
        const code = str(body?.code)?.trim();
        const repoPath = str(body?.repo_path)?.trim();
        if (!code || !repoPath) return sendError(c, 400, "code と repo_path は必須です");
        const project = str(body?.project)?.trim();
        return sendResult(c, "concordia",
            await deps.cc.registerProject({ code, repo_path: repoPath, project: project || undefined, added_by: deps.cfg.triggeredBy }),
            (data) => ({ registered: data }));
    });

    app.get("/api/templates", async (c) => sendResult(c, "concordia", await deps.cc.templates(), (templates) => ({ templates })));

    app.post("/api/projects/:code/instruct", async (c) => {
        const body = await readJson(c);
        const callName = str(body?.call_name);
        if (!callName) return sendError(c, 400, "call_name は必須です");
        const args = body?.args && typeof body.args === "object" && !Array.isArray(body.args)
            ? (body.args as Record<string, unknown>) : {};
        const out = await instruct(deps, c.req.param("code"), { callName, args, extraPrompt: str(body?.extra_prompt) });
        return out.ok ? c.json({ run: out.run }, 201) : c.json({ error: "instruct", message: out.reason }, out.status as 400);
    });

    app.get("/api/runs", async (c) => {
        const code = c.req.query("project")?.trim();
        let repoPath: string | null = null;
        if (code) {
            const projects = await deps.cc.projects();
            if (!projects.ok) return sendResult(c, "concordia", projects, () => ({}));
            repoPath = projects.data.find((p) => p.code === code)?.repo_path ?? null;
            if (!repoPath) return sendError(c, 404, `プロジェクト ${code} は Cc に登録されていません`);
        }
        return sendResult(c, "concordia", await deps.cc.runs(100), (runs) => ({
            runs: runs.filter((r) => r.triggered_by === deps.cfg.triggeredBy
                && (!repoPath || runBelongsTo(r, repoPath))),
        }));
    });

    app.get("/api/runs/:id", async (c) => sendResult(c, "concordia", await deps.cc.run(c.req.param("id")), (run) => ({ run })));

    return app;
}
