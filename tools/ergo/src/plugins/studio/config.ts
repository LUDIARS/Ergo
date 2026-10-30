// @implements SPEC-ergo-studio-endpoints
// @implements SPEC-ergo-studio-secrets
/// Studio の設定 (環境変数から一度だけ読む純関数)。
///
/// 接続先 URL はここに持たない (Excubitor の topology が正本、net/endpoints.ts)。
/// 例外は Curare: この拠点の catalog に載らない遠隔の Cu Server を使うことがあるため、
/// `ERGO_CURARE_URL` で明示できる (指定が無く topology にも無ければ「未接続」)。

import { studioHome } from "../../core/plugin-ledger.js";

export type ActioAuth =
    | { mode: "local" }                       // Actio の ACTIO_LOCAL_MODE (loopback 限定・トークン無し)
    | { mode: "token"; token: string };       // Cernere の Bearer トークン

export interface StudioConfig {
    home:            string;
    ludiarsRoot:     string | null;
    excubitorUrl:    string | null;           // 明示 (EXCUBITOR_URL)。無ければ catalog から解決
    actio:           ActioAuth;
    curareUrl:       string | null;
    curareToken:     string | null;
    /** プラグイン取得を許す GitHub owner (大文字小文字は区別しない)。 */
    pluginOwners:    string[];
    /** 委託の起点 (Cc の triggered_by)。 */
    triggeredBy:     string;
    requestTimeoutMs: number;
}

const DEFAULT_PLUGIN_OWNERS = ["LUDIARS"];

function nonEmpty(v: string | undefined): string | null {
    const t = v?.trim();
    return t ? t : null;
}

function stripSlash(url: string | null): string | null {
    return url ? url.replace(/\/+$/, "") : null;
}

export function loadStudioConfig(env: NodeJS.ProcessEnv = process.env): StudioConfig {
    const token = nonEmpty(env.ERGO_ACTIO_TOKEN);
    const owners = nonEmpty(env.ERGO_PLUGIN_OWNERS)
        ?.split(",").map((s) => s.trim()).filter(Boolean) ?? DEFAULT_PLUGIN_OWNERS;
    const timeout = Number(env.ERGO_STUDIO_TIMEOUT_MS);
    return {
        home:         studioHome(env),
        ludiarsRoot:  nonEmpty(env.LUDIARS_ROOT),
        excubitorUrl: stripSlash(nonEmpty(env.EXCUBITOR_URL)),
        actio:        token ? { mode: "token", token } : { mode: "local" },
        curareUrl:    stripSlash(nonEmpty(env.ERGO_CURARE_URL)),
        curareToken:  nonEmpty(env.ERGO_CURARE_TOKEN),
        pluginOwners: owners.length > 0 ? owners : DEFAULT_PLUGIN_OWNERS,
        triggeredBy:  "ergo-studio",
        requestTimeoutMs: Number.isFinite(timeout) && timeout > 0 ? timeout : 8000,
    };
}
