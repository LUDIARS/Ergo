// @implements SPEC-ergo-studio-tasks
/// タスク (Actio) とプロジェクト状態 (Br)、タスクからの実装依頼。
///   GET   /api/tasks?project=&done=1         一覧
///   POST  /api/tasks                         作成 { title, description?, priority?, projectId? }
///   PATCH /api/tasks/:id                     更新 { status?, title?, description?, priority? }
///   POST  /api/tasks/:id/implement           Cc に実装させる { code, call_name, extra_prompt? }
///   GET   /api/status                        Br の登録プロジェクト
///   GET   /api/status/:code                  Br の概要
///   POST  /api/status/:code/refresh          Br の取り直し

import { Hono } from "hono";

import type { ActioTaskInput } from "../clients/actio.js";
import type { StudioDeps } from "../deps.js";
import { implementTask } from "../usecases/instruct.js";
import { readJson, sendError, sendResult, str } from "./respond.js";

const STATUSES = new Set(["open", "in_progress", "blocked", "done", "cancelled"]);
const PRIORITIES = new Set(["low", "medium", "high", "critical"]);   // Actio の VALID_PRIORITIES

function taskInput(body: Record<string, unknown>): ActioTaskInput | string {
    const out: ActioTaskInput = {};
    const title = str(body.title)?.trim();
    if (title !== undefined) out.title = title;
    const description = str(body.description);
    if (description !== undefined) out.description = description;
    const status = str(body.status);
    if (status !== undefined) {
        if (!STATUSES.has(status)) return "status が不正です";
        out.status = status;
    }
    const priority = str(body.priority);
    if (priority !== undefined && priority !== "") {
        if (!PRIORITIES.has(priority)) return "priority が不正です";
        out.priority = priority;
    }
    const projectId = str(body.projectId)?.trim();
    if (projectId) out.projectId = projectId;
    return out;
}

export function taskRoutes(deps: StudioDeps): Hono {
    const app = new Hono();

    app.get("/api/tasks", async (c) => {
        const project = c.req.query("project")?.trim() || null;
        return sendResult(c, "actio", await deps.actio.tasks(project, c.req.query("done") === "1"), (tasks) => ({ tasks }));
    });

    app.post("/api/tasks", async (c) => {
        const body = await readJson(c);
        if (!body) return sendError(c, 400, "JSON で送ってください");
        const input = taskInput(body);
        if (typeof input === "string") return sendError(c, 400, input);
        if (!input.title) return sendError(c, 400, "title は必須です");
        return sendResult(c, "actio", await deps.actio.create(input), (task) => ({ task }));
    });

    app.patch("/api/tasks/:id", async (c) => {
        const body = await readJson(c);
        if (!body) return sendError(c, 400, "JSON で送ってください");
        const input = taskInput(body);
        if (typeof input === "string") return sendError(c, 400, input);
        return sendResult(c, "actio", await deps.actio.update(c.req.param("id"), input), (task) => ({ task }));
    });

    app.post("/api/tasks/:id/implement", async (c) => {
        const body = await readJson(c);
        const code = str(body?.code)?.trim();
        const callName = str(body?.call_name);
        if (!code || !callName) return sendError(c, 400, "code と call_name は必須です");
        const out = await implementTask(deps, code, c.req.param("id"), callName, str(body?.extra_prompt));
        return out.ok ? c.json({ run: out.run }, 201) : c.json({ error: "implement", message: out.reason }, out.status as 400);
    });

    app.get("/api/status", async (c) => sendResult(c, "breviarium", await deps.br.projects(), (projects) => ({ projects })));
    app.get("/api/status/:code", async (c) =>
        sendResult(c, "breviarium", await deps.br.overview(c.req.param("code")), (overview) => ({ overview })));
    app.post("/api/status/:code/refresh", async (c) =>
        sendResult(c, "breviarium", await deps.br.refresh(c.req.param("code")), (result) => ({ result })));

    return app;
}
