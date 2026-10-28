// @implements SPEC-ergo-studio-resources
/// Curare (Cu) — アセットの版・提出と、選択した版の取得計画。
/// 認証は Cernere 由来の Bearer トークン (`ERGO_CURARE_TOKEN`)。トークンは保存も画面表示もしない。

import type { EndpointResolver } from "../net/endpoints.js";
import { requestJson, type ServiceResult } from "../net/http.js";

export interface CuProject { id: string; name: string; [key: string]: unknown }
export interface CuFolder  { id: string; name: string; [key: string]: unknown }
export interface CuAsset   { id: string; name: string; versioningType?: string; [key: string]: unknown }
export interface CuVersion {
    id:          string;
    versionNum:  number;
    contentHash: string;
    fileSize:    number;
    createdAt?:  string;
    comment?:    string | null;
    [key: string]: unknown;
}

export interface CuPlanItem {
    versionId:    string;
    resourceId:   string;
    name:         string;
    resourceRole?: string | null;
    fileSize:     number;
    contentHash:  string;
    needsDownload: boolean;
    downloadUrl:  string | null;
}

export interface CuPlan {
    items:         CuPlanItem[];
    transferBytes: number;
    overwrite:     false;
}

function listOf<T>(data: unknown, key: string): T[] {
    if (Array.isArray(data)) return data as T[];
    const v = (data as Record<string, unknown> | null)?.[key];
    return Array.isArray(v) ? (v as T[]) : [];
}

export class CurareClient {
    constructor(private readonly ep: EndpointResolver, private readonly token: string | null,
                private readonly timeoutMs: number) {}

    get hasToken(): boolean {
        return this.token !== null;
    }

    private headers(): Record<string, string> {
        return this.token ? { authorization: `Bearer ${this.token}` } : {};
    }

    private async get<T>(path: string): Promise<ServiceResult<T>> {
        return requestJson<T>(await this.ep.url("curare"), path, { headers: this.headers(), timeoutMs: this.timeoutMs });
    }

    async me(): Promise<ServiceResult<unknown>> {
        return this.get("/api/v1/me");
    }

    async projects(): Promise<ServiceResult<CuProject[]>> {
        const r = await this.get<unknown>("/api/v1/projects");
        return r.ok ? { ...r, data: listOf<CuProject>(r.data, "projects") } : r;
    }

    async folders(projectId: string): Promise<ServiceResult<CuFolder[]>> {
        const r = await this.get<unknown>(`/api/v1/projects/${encodeURIComponent(projectId)}/folders`);
        return r.ok ? { ...r, data: listOf<CuFolder>(r.data, "folders") } : r;
    }

    async assets(projectId: string, folderId: string): Promise<ServiceResult<CuAsset[]>> {
        const r = await this.get<unknown>(
            `/api/v1/projects/${encodeURIComponent(projectId)}/folders/${encodeURIComponent(folderId)}/assets`);
        return r.ok ? { ...r, data: listOf<CuAsset>(r.data, "assets") } : r;
    }

    async versions(assetId: string): Promise<ServiceResult<CuVersion[]>> {
        const r = await this.get<unknown>(`/api/v1/assets/${encodeURIComponent(assetId)}/versions`);
        return r.ok ? { ...r, data: listOf<CuVersion>(r.data, "versions") } : r;
    }

    /// 選んだ版 (依存の閉包を含む) の取得計画。手元にある内容ハッシュは送り、取得を省かせる。
    async plan(versionIds: string[], knownHashes: string[]): Promise<ServiceResult<CuPlan>> {
        return requestJson<CuPlan>(await this.ep.url("curare"), "/api/v1/acquisition/plan", {
            method: "POST", body: { versionIds, knownHashes }, headers: this.headers(), timeoutMs: this.timeoutMs,
        });
    }
}
