// @implements SPEC-ergo-studio-endpoints
/// 接続先の解決。ポート番号の正本は Excubitor の catalog で、コードには持たない。
///
///   1. Excubitor 自身: `EXCUBITOR_URL` (Excubitor が注入する) → 無ければ
///      `<LUDIARS_ROOT>/Excubitor/excubitor.catalog.yaml` の `code: excubitor` の port。
///   2. 他のサービス: Excubitor の `GET /api/v1/topology` が返す `<CODE>_URL`。
///   3. Curare だけは `ERGO_CURARE_URL` の明示を優先する (遠隔の Cu Server)。
///
/// 解決できなければ null (= 未接続)。別のポートや別ホストを試すことはしない。

import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";

import type { StudioConfig } from "../config.js";
import { requestJson } from "./http.js";

export type ServiceKey = "concordia" | "actio" | "breviarium" | "curare" | "revisor" | "excubitor" | "cernere";

/// topology の env 名。
export const TOPOLOGY_KEYS: Record<ServiceKey, string> = {
    concordia:  "CONCORDIA_URL",
    actio:      "ACTIO_URL",
    breviarium: "BREVIARIUM_URL",
    curare:     "CURARE_URL",
    revisor:    "REVISOR_URL",
    excubitor:  "EXCUBITOR_URL",
    cernere:    "CERNERE_URL",
};

/// catalog (YAML) から `code: excubitor` の本体 port を読む。
/// サービス直下 (インデント 4) の `port:` だけを見る (`ports:` 配下の拠点間 port は無視)。
export function excubitorPortFromCatalog(yaml: string): number | null {
    let current: string | null = null;
    for (const line of yaml.split(/\r?\n/)) {
        const code = line.match(/^\s*-\s+code:\s*([^\s#]+)/);
        if (code) {
            current = code[1]!;
            continue;
        }
        if (current !== "excubitor") continue;
        const port = line.match(/^ {4}port:\s*(\d+)\s*(#.*)?$/);
        if (port) return Number(port[1]);
    }
    return null;
}

export function resolveExcubitorUrl(cfg: StudioConfig): string | null {
    if (cfg.excubitorUrl) return cfg.excubitorUrl;
    if (!cfg.ludiarsRoot) return null;
    const path = join(cfg.ludiarsRoot, "Excubitor", "excubitor.catalog.yaml");
    if (!existsSync(path)) return null;
    const port = excubitorPortFromCatalog(readFileSync(path, "utf8"));
    return port ? `http://127.0.0.1:${port}` : null;
}

export interface EndpointSnapshot {
    resolvedAt: string;
    /** topology を取れたか (取れなければ Excubitor 以外は未解決)。 */
    topology:   "ok" | "unavailable" | "not_configured";
    urls:       Record<ServiceKey, string | null>;
    /** topology に載っている全サービス (`<CODE>_URL`)。可視化で使う。 */
    all:        Record<string, string>;
}

/// topology env から Studio が使うサービスの URL を選ぶ (純関数)。
export function pickEndpoints(env: Record<string, string>, cfg: Pick<StudioConfig, "curareUrl">,
                              excubitorUrl: string | null): Pick<EndpointSnapshot, "urls" | "all"> {
    const strip = (u: string | undefined) => (u ? u.replace(/\/+$/, "") : null);
    const urls = {} as Record<ServiceKey, string | null>;
    for (const key of Object.keys(TOPOLOGY_KEYS) as ServiceKey[]) {
        urls[key] = strip(env[TOPOLOGY_KEYS[key]]);
    }
    urls.excubitor = excubitorUrl;
    if (cfg.curareUrl) urls.curare = cfg.curareUrl;
    const all: Record<string, string> = {};
    for (const [k, v] of Object.entries(env)) {
        if (k.endsWith("_URL") && typeof v === "string") all[k.slice(0, -4)] = v;
    }
    return { urls, all };
}

/// 30 秒だけ覚える解決器。Excubitor が落ちていても直近の成功結果を使い続けない
/// (古い接続先で書き込まないよう、失敗したら未解決に戻す)。
export class EndpointResolver {
    private cache: { at: number; snap: EndpointSnapshot } | null = null;

    constructor(private readonly cfg: StudioConfig, private readonly ttlMs = 30_000) {}

    async snapshot(force = false): Promise<EndpointSnapshot> {
        if (!force && this.cache && Date.now() - this.cache.at < this.ttlMs) return this.cache.snap;
        const ex = resolveExcubitorUrl(this.cfg);
        const empty = pickEndpoints({}, this.cfg, ex);
        let snap: EndpointSnapshot;
        if (!ex) {
            snap = { resolvedAt: new Date().toISOString(), topology: "not_configured", ...empty };
        } else {
            const r = await requestJson<{ env?: Record<string, string> }>(ex, "/api/v1/topology",
                { timeoutMs: this.cfg.requestTimeoutMs });
            snap = r.ok && r.data?.env
                ? { resolvedAt: new Date().toISOString(), topology: "ok", ...pickEndpoints(r.data.env, this.cfg, ex) }
                : { resolvedAt: new Date().toISOString(), topology: "unavailable", ...empty };
        }
        this.cache = { at: Date.now(), snap };
        return snap;
    }

    async url(key: ServiceKey): Promise<string | null> {
        return (await this.snapshot()).urls[key];
    }
}
