// @implements SPEC-ergo-studio-connections
/// 各サービスとの連携を確認する (読み取りだけ)。結果は連携図 (domain/connection-map.ts) の元。

import type { StudioDeps } from "../deps.js";
import { buildConnectionMap, type ProbeResult } from "../domain/connection-map.js";
import { requestJson, type ServiceResult } from "../net/http.js";
import type { ServiceKey } from "../net/endpoints.js";

export class ConnectionProber {
    private readonly lastOk = new Map<ServiceKey, string>();

    constructor(private readonly deps: StudioDeps) {}

    private record(key: ServiceKey, url: string | null, r: ServiceResult<unknown>, auth?: string): ProbeResult {
        const now = new Date().toISOString();
        if (r.ok) this.lastOk.set(key, now);
        return {
            key, url, ok: r.ok, failure: r.ok ? undefined : r.kind, detail: r.ok ? undefined : r.message,
            checkedAt: now, lastOkAt: this.lastOk.get(key) ?? null, auth,
        };
    }

    async probe() {
        const { cfg, endpoints, cc, actio, br, cu, revisor } = this.deps;
        const snap = await endpoints.snapshot(true);
        const t = cfg.requestTimeoutMs;
        const exResult: ServiceResult<unknown> = snap.topology === "ok"
            ? { ok: true, data: null, status: 200 }
            : { ok: false, kind: snap.topology === "not_configured" ? "not_configured" : "unreachable",
                message: snap.topology === "not_configured"
                    ? "EXCUBITOR_URL も LUDIARS_ROOT の catalog も無く、接続先を解決できません"
                    : "Excubitor の topology を取得できません" };

        const revisorResult: ServiceResult<unknown> = revisor.available
            ? await revisor.ping().then((r) => r.ok
                ? { ok: true as const, data: null, status: 200 }
                : { ok: false as const, kind: "server_error" as const, message: r.message })
            : { ok: false, kind: "not_configured", message: "Revisor の CLI が見つかりません" };

        const [ccR, actioR, brR, cuR, crR] = await Promise.all([
            cc.projects(),
            actio.me(),
            br.health(),
            cu.hasToken ? cu.me() : Promise.resolve<ServiceResult<unknown>>(
                snap.urls.curare
                    ? { ok: false, kind: "unauthorized", message: "ERGO_CURARE_TOKEN が未設定です" }
                    : { ok: false, kind: "not_configured", message: "Curare の接続先がありません (ERGO_CURARE_URL)" }),
            requestJson(snap.urls.cernere, "/", { timeoutMs: t }).then((r) =>
                r.ok || (r.kind !== "unreachable" && r.kind !== "timeout" && r.kind !== "not_configured")
                    ? { ok: true as const, data: null, status: 200 } : r),
        ]);

        const probes: ProbeResult[] = [
            this.record("excubitor", snap.urls.excubitor, exResult),
            this.record("concordia", snap.urls.concordia, ccR, "loopback"),
            this.record("actio", snap.urls.actio, actioR, cfg.actio.mode === "token" ? "Cernere トークン" : "local mode"),
            this.record("breviarium", snap.urls.breviarium, brR, "loopback"),
            this.record("curare", snap.urls.curare, cuR, cu.hasToken ? "Cernere トークン" : "未設定"),
            this.record("revisor", snap.urls.revisor, revisorResult, "CLI"),
            this.record("cernere", snap.urls.cernere, crR),
        ];
        const others = Object.entries(snap.all)
            .map(([name, url]) => ({ name, url }))
            .sort((a, b) => a.name.localeCompare(b.name));
        return { resolvedAt: snap.resolvedAt, topology: snap.topology, ...buildConnectionMap(probes), probes, others };
    }
}
