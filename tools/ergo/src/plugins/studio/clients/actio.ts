// @implements SPEC-ergo-studio-tasks
/// Actio — タスクの一覧・作成・更新。
/// 認証は Actio の local mode (loopback・トークン無し) か Cernere の Bearer トークン
/// (config.ts の ActioAuth)。ブラウザからではなく Ergo のサーバから呼ぶ (Origin を付けない)。

import type { ActioAuth } from "../config.js";
import type { EndpointResolver } from "../net/endpoints.js";
import { requestJson, type ServiceResult } from "../net/http.js";

export interface ActioTask {
    id:          string;
    title:       string;
    status:      string;
    priority?:   string | null;
    projectId?:  string | null;
    category?:   string | null;
    deadline?:   string | null;
    description?: string | null;
    updatedAt?:  string;
}

export interface ActioTaskInput {
    title?:       string;
    description?: string;
    status?:      string;
    priority?:    string;
    projectId?:   string;
    deadline?:    string | null;
}

export class ActioClient {
    constructor(private readonly ep: EndpointResolver, private readonly auth: ActioAuth,
                private readonly timeoutMs: number) {}

    private headers(): Record<string, string> {
        return this.auth.mode === "token" ? { authorization: `Bearer ${this.auth.token}` } : {};
    }

    private async base(): Promise<string | null> {
        return this.ep.url("actio");
    }

    /// プロジェクト (Cc の code) で絞ったタスク。scope=all は Actio の権限内で全件。
    async tasks(project: string | null, includeDone: boolean): Promise<ServiceResult<ActioTask[]>> {
        const q = new URLSearchParams({ scope: "all", include: "description", limit: "200" });
        if (project) q.set("project", project);
        if (!includeDone) q.set("status", "open,in_progress,blocked");
        const r = await requestJson<{ tasks?: ActioTask[] }>(await this.base(), `/api/tasks?${q}`,
            { headers: this.headers(), timeoutMs: this.timeoutMs });
        return r.ok ? { ...r, data: r.data?.tasks ?? [] } : r;
    }

    async task(id: string): Promise<ServiceResult<ActioTask>> {
        const r = await requestJson<{ task?: ActioTask } & ActioTask>(await this.base(),
            `/api/tasks/${encodeURIComponent(id)}`, { headers: this.headers(), timeoutMs: this.timeoutMs });
        return r.ok ? { ...r, data: r.data?.task ?? r.data } : r;
    }

    async create(input: ActioTaskInput): Promise<ServiceResult<ActioTask>> {
        const r = await requestJson<{ task?: ActioTask } & ActioTask>(await this.base(), "/api/tasks",
            { method: "POST", body: input, headers: this.headers(), timeoutMs: this.timeoutMs });
        return r.ok ? { ...r, data: r.data?.task ?? r.data } : r;
    }

    async update(id: string, input: ActioTaskInput): Promise<ServiceResult<ActioTask>> {
        const r = await requestJson<{ task?: ActioTask } & ActioTask>(await this.base(),
            `/api/tasks/${encodeURIComponent(id)}`,
            { method: "PATCH", body: input, headers: this.headers(), timeoutMs: this.timeoutMs });
        return r.ok ? { ...r, data: r.data?.task ?? r.data } : r;
    }

    /// 接続と認証の確認 (local mode なら localMode: true が返る)。
    async me(): Promise<ServiceResult<{ localMode?: boolean }>> {
        return requestJson(await this.base(), "/api/auth/me", { headers: this.headers(), timeoutMs: this.timeoutMs });
    }
}
