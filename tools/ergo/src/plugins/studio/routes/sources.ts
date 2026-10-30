// @implements SPEC-ergo-studio-sources
/// ソース (git) の状態と Revisor の PR、同期。
///   GET  /api/sources                       全登録プロジェクトの git 状態 (並列 4)
///   GET  /api/sources/:code                 1 件の git 状態 + Revisor の PR
///   POST /api/sources/:code/fetch           git fetch
///   POST /api/sources/:code/fast-forward    main の早送り (clean な main のときだけ)

import { Hono } from "hono";

import type { CcProject } from "../clients/concordia.js";
import type { StudioDeps } from "../deps.js";
import { repoSlugFromOrigin } from "../domain/delegation-request.js";
import { decideFastForward } from "../domain/source-sync.js";
import { sendError } from "./respond.js";

async function mapLimited<T, R>(items: T[], limit: number, fn: (t: T) => Promise<R>): Promise<R[]> {
    const out: R[] = new Array(items.length);
    let next = 0;
    const workers = Array.from({ length: Math.min(limit, items.length) }, async () => {
        while (next < items.length) {
            const i = next++;
            out[i] = await fn(items[i]!);
        }
    });
    await Promise.all(workers);
    return out;
}

export function sourceRoutes(deps: StudioDeps): Hono {
    const app = new Hono();

    async function findProject(code: string): Promise<CcProject | string> {
        const r = await deps.cc.projects();
        if (!r.ok) return `Concordia: ${r.message}`;
        return r.data.find((p) => p.code === code) ?? `プロジェクト ${code} は Cc に登録されていません`;
    }

    app.get("/api/sources", async (c) => {
        const r = await deps.cc.projects();
        if (!r.ok) return c.json({ error: r.kind, service: "concordia", message: r.message }, 503);
        const rows = await mapLimited(r.data, 4, async (p) => {
            const st = await deps.git.status(p.repo_path);
            return { code: p.code, project: p.project, ...(st.ok ? { status: st.data } : { error: st.message }) };
        });
        return c.json({ sources: rows });
    });

    app.get("/api/sources/:code", async (c) => {
        const project = await findProject(c.req.param("code"));
        if (typeof project === "string") return sendError(c, 404, project);
        const slug = repoSlugFromOrigin(project.repo_origin);
        const [st, prs] = await Promise.all([
            deps.git.status(project.repo_path),
            slug ? deps.revisor.prs(slug) : Promise.resolve({ ok: false as const, message: "repo origin が未登録です" }),
        ]);
        return c.json({
            code: project.code,
            repository: slug,
            workflow: project.revisor_workflow ?? null,
            status: st.ok ? st.data : null,
            statusError: st.ok ? null : st.message,
            fastForward: st.ok ? decideFastForward(st.data) : null,
            prs: prs.ok ? prs.data.slice(0, 50) : [],
            prsError: prs.ok ? null : prs.message,
        });
    });

    app.post("/api/sources/:code/fetch", async (c) => {
        const project = await findProject(c.req.param("code"));
        if (typeof project === "string") return sendError(c, 404, project);
        const r = await deps.git.fetch(project.repo_path);
        return r.ok ? c.json({ ok: true, message: r.message }) : sendError(c, 502, r.message);
    });

    app.post("/api/sources/:code/fast-forward", async (c) => {
        const project = await findProject(c.req.param("code"));
        if (typeof project === "string") return sendError(c, 404, project);
        // 判定は送る直前の状態で行う (画面に出ていた状態は古いかもしれない)。
        const st = await deps.git.status(project.repo_path);
        if (!st.ok) return sendError(c, 502, st.message);
        const decision = decideFastForward(st.data);
        if (!decision.allowed) return sendError(c, 409, decision.reason);
        const r = await deps.git.fastForward(project.repo_path, decision.branch);
        return r.ok ? c.json({ ok: true, message: r.message }) : sendError(c, 502, r.message);
    });

    return app;
}
