// @implements SPEC-ergo-studio-instruct
// @implements SPEC-ergo-studio-tasks
/// AI への指示 — Cc の登録プロジェクトとテンプレートを引き直してから委託する。
/// 画面が持っている古い一覧ではなく、送る直前の Cc の正本で検証する。

import type { CcInvokeResponse } from "../clients/concordia.js";
import type { StudioDeps } from "../deps.js";
import { buildInvokeRequest, taskPrompt, type InstructionInput } from "../domain/delegation-request.js";

export type InstructOutcome =
    | { ok: true; run: CcInvokeResponse }
    | { ok: false; status: number; reason: string };

export async function instruct(deps: StudioDeps, code: string, input: InstructionInput): Promise<InstructOutcome> {
    const [projects, templates] = await Promise.all([deps.cc.projects(), deps.cc.templates()]);
    if (!projects.ok) return { ok: false, status: 502, reason: `Concordia: ${projects.message}` };
    if (!templates.ok) return { ok: false, status: 502, reason: `Concordia: ${templates.message}` };
    const project = projects.data.find((p) => p.code === code);
    if (!project) return { ok: false, status: 404, reason: `プロジェクト ${code} は Cc に登録されていません` };
    const template = templates.data.find((t) => t.call_name === input.callName);
    if (!template) return { ok: false, status: 404, reason: `テンプレート ${input.callName} がありません (無効化を含む)` };
    const built = buildInvokeRequest(project, template, input, deps.cfg.triggeredBy);
    if (!built.ok) return { ok: false, status: 400, reason: built.reason };
    const r = await deps.cc.invoke(built.request);
    if (!r.ok) return { ok: false, status: r.status && r.status < 500 ? r.status : 502, reason: `Concordia: ${r.message}` };
    return { ok: true, run: r.data };
}

/// Actio のタスクを AI に実装させる。タスク本文をテンプレートの `task` 入力に入れ、
/// `taskflow_reference` で同じタスクに結びつける (Cc は新しいタスクを作らない)。
export async function implementTask(deps: StudioDeps, code: string, taskId: string,
                                    callName: string, extraPrompt?: string): Promise<InstructOutcome> {
    const task = await deps.actio.task(taskId);
    if (!task.ok) return { ok: false, status: task.status === 404 ? 404 : 502, reason: `Actio: ${task.message}` };
    return instruct(deps, code, {
        callName,
        args: {},
        taskText: taskPrompt(task.data),
        extraPrompt,
        actioTaskId: task.data.id,
    });
}
