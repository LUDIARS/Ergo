// @implements SPEC-ergo-studio-tasks
/// Breviarium (Br) — プロジェクトの状態 (スタートアップ / スプリント / PDCA / クラス評価) の要約。
/// Br は自分のキャッシュから返す。ソースへ取りに行くのは refresh だけ。

import type { EndpointResolver } from "../net/endpoints.js";
import { requestJson, type ServiceResult } from "../net/http.js";

export interface BrProject {
    code:  string;
    name?: string;
    [key: string]: unknown;
}

export class BreviariumClient {
    constructor(private readonly ep: EndpointResolver, private readonly timeoutMs: number) {}

    private async base(): Promise<string | null> {
        return this.ep.url("breviarium");
    }

    async projects(): Promise<ServiceResult<BrProject[]>> {
        const r = await requestJson<{ projects?: BrProject[] } | BrProject[]>(await this.base(), "/api/projects",
            { timeoutMs: this.timeoutMs });
        if (!r.ok) return r;
        const list = Array.isArray(r.data) ? r.data : r.data?.projects ?? [];
        return { ...r, data: list };
    }

    async overview(code: string): Promise<ServiceResult<unknown>> {
        return requestJson(await this.base(), `/api/projects/${encodeURIComponent(code)}/overview`,
            { timeoutMs: this.timeoutMs });
    }

    async refresh(code: string): Promise<ServiceResult<unknown>> {
        return requestJson(await this.base(), `/api/projects/${encodeURIComponent(code)}/refresh`,
            { method: "POST", body: {}, timeoutMs: this.timeoutMs * 4 });
    }

    async health(): Promise<ServiceResult<unknown>> {
        return requestJson(await this.base(), "/health", { timeoutMs: this.timeoutMs });
    }
}
