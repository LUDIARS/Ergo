// @implements SPEC-ergo-studio-instruct
// @implements SPEC-ergo-studio-tasks
/// AI への指示 (Cc の delegation invoke) を組み立てる (純関数)。
///
/// - 作業場所 (cwd) は Cc に登録されたプロジェクトの repo_path に固定する。画面から任意の
///   パスを渡させない (登録外のディレクトリで AI を動かさない)。
/// - テンプレートの必須入力が欠けていれば送らずに理由を返す。`target_repo` は repo_path で補う。
/// - Actio のタスクから指示する場合は `taskflow_reference = actio:<id>` で既存タスクに結びつけ、
///   Cc に新しいタスクを作らせない。

import type { CcInvokeRequest, CcProject, CcTemplate } from "../clients/concordia.js";

export interface InstructionInput {
    callName:     string;
    args:         Record<string, unknown>;
    extraPrompt?: string;
    actioTaskId?: string;
    /** 依頼本文 (Actio タスクなど)。テンプレートに `task` 入力があればそこへ、無ければ追加指示の先頭へ。 */
    taskText?:    string;
}

export type BuildResult =
    | { ok: true; request: CcInvokeRequest }
    | { ok: false; reason: string };

const ACTIO_ID = /^[0-9a-zA-Z][0-9a-zA-Z_-]{0,127}$/;
const MAX_EXTRA_PROMPT = 20000;

export function buildInvokeRequest(project: CcProject, template: CcTemplate, input: InstructionInput,
                                   triggeredBy: string): BuildResult {
    if (template.call_name !== input.callName) return { ok: false, reason: "テンプレートが一致しません" };
    const args: Record<string, unknown> = {};
    const fields = template.input_schema ?? [];
    const taskText = input.taskText?.trim();
    const taskInSchema = fields.some((f) => f.name === "task");
    for (const field of fields) {
        let raw = input.args[field.name];
        if (field.name === "task" && taskText && (raw === undefined || raw === "")) raw = taskText;
        let value = typeof raw === "string" ? raw.trim() : raw;
        if ((value === undefined || value === "") && field.name === "target_repo") value = project.repo_path;
        if (value === undefined || value === "") {
            if (field.required) return { ok: false, reason: `必須の入力「${field.name}」が空です` };
            continue;
        }
        args[field.name] = value;
    }
    const extraParts = [taskText && !taskInSchema ? taskText : "", input.extraPrompt?.trim() ?? ""].filter(Boolean);
    const extra = extraParts.join("\n\n");
    if (extra && extra.length > MAX_EXTRA_PROMPT) {
        return { ok: false, reason: `追加指示は ${MAX_EXTRA_PROMPT} 文字までです` };
    }
    const request: CcInvokeRequest = {
        call_name:    template.call_name,
        args,
        cwd:          project.repo_path,
        triggered_by: triggeredBy,
        project:      project.code,
    };
    if (extra) request.extra_prompt = extra;
    if (input.actioTaskId !== undefined) {
        if (!ACTIO_ID.test(input.actioTaskId)) return { ok: false, reason: "Actio のタスク ID が不正です" };
        request.taskflow_reference = `actio:${input.actioTaskId}`;
    }
    return { ok: true, request };
}

/// Actio のタスクを実装依頼の本文にする (テンプレートの `task` 入力へ入れる)。
export function taskPrompt(task: { id: string; title: string; description?: string | null }): string {
    const body = task.description?.trim();
    return body ? `${task.title}\n\n${body}\n\n(Actio: actio:${task.id})` : `${task.title}\n\n(Actio: actio:${task.id})`;
}

function samePath(a: string, b: string): boolean {
    const norm = (p: string) => p.replace(/\\/g, "/").replace(/\/+$/, "").toLowerCase();
    return norm(a) === norm(b);
}

/// 委託 run がプロジェクトのものか (Studio は cwd と target_repo を repo_path にして送る)。
export function runBelongsTo(run: { cwd?: string | null; args?: Record<string, unknown> }, repoPath: string): boolean {
    if (typeof run.cwd === "string" && samePath(run.cwd, repoPath)) return true;
    const target = run.args?.target_repo;
    return typeof target === "string" && samePath(target, repoPath);
}

/// `git@github.com:Owner/Repo.git` / `https://github.com/Owner/Repo(.git)` → `Owner/Repo`。
export function repoSlugFromOrigin(origin: string | null | undefined): string | null {
    if (!origin) return null;
    const m = origin.trim().match(/github\.com[:/]([^/\s]+)\/([^/\s]+?)(?:\.git)?\/?$/i);
    return m ? `${m[1]}/${m[2]}` : null;
}
