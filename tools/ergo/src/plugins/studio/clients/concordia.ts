// @implements SPEC-ergo-studio-instruct
/// Concordia (Cc) — 登録プロジェクトと AI への実装指示 (delegation)。
/// Cc の HTTP は loopback 専用の管理 API (spec/feature/trust-boundaries.md)。

import type { EndpointResolver } from "../net/endpoints.js";
import { requestJson, type ServiceResult } from "../net/http.js";

export interface CcProject {
    code:        string;
    project:     string;
    repo_path:   string;
    repo_origin?: string | null;
    revisor_workflow?: string | null;
}

export interface CcTemplateField {
    name:         string;
    type:         string;
    required?:    boolean;
    description?: string;
}

export interface CcTemplate {
    call_name:       string;
    title:           string;
    description:     string | null;
    target_provider: string;
    model:           string | null;
    input_schema:    CcTemplateField[];
    is_active:       number | boolean;
    call_only?:      number | boolean;
    category?:       string | null;
}

export interface CcInvokeRequest {
    call_name:           string;
    args:                Record<string, unknown>;
    cwd?:                string;
    extra_prompt?:       string;
    triggered_by:        string;
    project?:            string;
    taskflow_reference?: string;
}

export interface CcInvokeResponse {
    run_id:           string;
    prompt_file_path?: string | null;
    spawn_pid?:       number | null;
}

export interface CcRun {
    id:           string;
    call_name:    string;
    status:       string;
    triggered_by: string | null;
    created_at:   number;
    args?:        Record<string, unknown>;
    cwd?:         string | null;
}

export class ConcordiaClient {
    constructor(private readonly ep: EndpointResolver, private readonly timeoutMs: number) {}

    private async base(): Promise<string | null> {
        return this.ep.url("concordia");
    }

    /// 管理用の一覧 (repo origin と Revisor workflow を含む。Revisor を待たずに返る)。
    async projects(): Promise<ServiceResult<CcProject[]>> {
        const r = await requestJson<{ entries?: CcProject[] }>(await this.base(), "/v1/project-codes/admin",
            { timeoutMs: this.timeoutMs });
        return r.ok ? { ...r, data: r.data?.entries ?? [] } : r;
    }

    async registerProject(input: { code: string; repo_path: string; project?: string; added_by: string }):
        Promise<ServiceResult<unknown>> {
        return requestJson(await this.base(), "/v1/project-codes",
            { method: "POST", body: input, timeoutMs: this.timeoutMs });
    }

    async templates(): Promise<ServiceResult<CcTemplate[]>> {
        const r = await requestJson<{ templates?: CcTemplate[] }>(await this.base(), "/v1/delegation/templates",
            { timeoutMs: this.timeoutMs });
        return r.ok ? { ...r, data: (r.data?.templates ?? []).filter((t) => Boolean(t.is_active)) } : r;
    }

    async invoke(req: CcInvokeRequest): Promise<ServiceResult<CcInvokeResponse>> {
        return requestJson(await this.base(), "/v1/delegation/invoke",
            { method: "POST", body: req, timeoutMs: this.timeoutMs * 4 });
    }

    async runs(limit = 100): Promise<ServiceResult<CcRun[]>> {
        const r = await requestJson<{ runs?: CcRun[] }>(await this.base(), `/v1/delegation/runs?limit=${limit}`,
            { timeoutMs: this.timeoutMs });
        return r.ok ? { ...r, data: r.data?.runs ?? [] } : r;
    }

    async run(id: string): Promise<ServiceResult<CcRun>> {
        const r = await requestJson<{ run?: CcRun } & CcRun>(await this.base(),
            `/v1/delegation/runs/${encodeURIComponent(id)}`, { timeoutMs: this.timeoutMs });
        return r.ok ? { ...r, data: r.data?.run ?? r.data } : r;
    }
}
