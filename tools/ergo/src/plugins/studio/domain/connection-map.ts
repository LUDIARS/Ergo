// @implements SPEC-ergo-studio-connections
/// 連携の可視化 — 各サービスの確認結果から図 (ノードと辺) を組み立てる (純関数)。
///
/// Studio が直接話すサービスを中心に置き、サービス同士の既知の連携 (Cc ⇄ Actio の
/// プロジェクト / タスク同期、Br → Actio のスプリント集計、Cu → Cr の認可など) を辺で示す。
/// topology に載っている他のサービスは「その他」として一覧だけ出す。

import type { FailureKind } from "../net/http.js";
import type { ServiceKey } from "../net/endpoints.js";

export type NodeState = "ok" | "degraded" | "down" | "not_configured" | "unauthorized";

export interface ProbeResult {
    key:       ServiceKey;
    url:       string | null;
    ok:        boolean;
    failure?:  FailureKind;
    detail?:   string;
    checkedAt: string;
    /** 直近で成功した時刻 (この Ergo プロセスが覚えている範囲)。 */
    lastOkAt:  string | null;
    /** 認証の方式 (表示用。秘密は含めない)。 */
    auth?:     string;
}

export interface MapNode {
    id:     ServiceKey | "studio";
    label:  string;
    role:   string;
    state:  NodeState;
    url:    string | null;
    detail: string | null;
    auth:   string | null;
    lastOkAt: string | null;
}

export interface MapEdge {
    from:  MapNode["id"];
    to:    MapNode["id"];
    label: string;
    /** 両端が動いていれば active。 */
    active: boolean;
}

export const SERVICE_INFO: Record<ServiceKey, { label: string; role: string }> = {
    excubitor:  { label: "Excubitor (Ex)",   role: "接続先 (catalog / topology) と起動管理" },
    concordia:  { label: "Concordia (Cc)",   role: "登録プロジェクトと AI への実装指示" },
    actio:      { label: "Actio (A)",        role: "タスク管理" },
    breviarium: { label: "Breviarium (Br)",  role: "プロジェクト状態の要約" },
    curare:     { label: "Curare (Cu)",      role: "リソースの版・提出・取得" },
    revisor:    { label: "Revisor",          role: "ソースの local PR・審査・マージ" },
    cernere:    { label: "Cernere (Cr)",     role: "認証・所属" },
};

/// Studio → 各サービス の用途。
const STUDIO_EDGES: Array<[ServiceKey, string]> = [
    ["excubitor",  "接続先の解決"],
    ["concordia",  "プロジェクト / 実装指示"],
    ["actio",      "タスク"],
    ["breviarium", "状態の要約"],
    ["curare",     "リソース同期"],
    ["revisor",    "PR の状態"],
];

/// サービス同士の既知の連携 (各サービスの仕様にある向き)。
const SERVICE_EDGES: Array<[ServiceKey, ServiceKey, string]> = [
    ["actio",      "concordia", "プロジェクト同期"],
    ["concordia",  "actio",     "タスク登録・状態"],
    ["concordia",  "revisor",   "local PR 提出"],
    ["breviarium", "actio",     "スプリント集計"],
    ["curare",     "cernere",   "認可・所属"],
    ["actio",      "cernere",   "認証 (トークン時)"],
];

export function stateOf(p: ProbeResult | undefined): NodeState {
    if (!p) return "not_configured";
    if (p.ok) return "ok";
    switch (p.failure) {
        case "not_configured": return "not_configured";
        case "unauthorized":
        case "forbidden":      return "unauthorized";
        case "rejected":
        case "not_found":
        case "bad_response":   return "degraded";
        default:               return "down";
    }
}

export function buildConnectionMap(probes: ProbeResult[]): { nodes: MapNode[]; edges: MapEdge[] } {
    const byKey = new Map(probes.map((p) => [p.key, p]));
    const nodes: MapNode[] = [{
        id: "studio", label: "Ergo Studio", role: "このアプリ", state: "ok",
        url: null, detail: null, auth: null, lastOkAt: null,
    }];
    for (const key of Object.keys(SERVICE_INFO) as ServiceKey[]) {
        const p = byKey.get(key);
        nodes.push({
            id: key, label: SERVICE_INFO[key].label, role: SERVICE_INFO[key].role, state: stateOf(p),
            url: p?.url ?? null, detail: p?.detail ?? null, auth: p?.auth ?? null, lastOkAt: p?.lastOkAt ?? null,
        });
    }
    const alive = (id: MapNode["id"]) => nodes.find((n) => n.id === id)?.state === "ok";
    const edges: MapEdge[] = [
        ...STUDIO_EDGES.map(([to, label]) => ({ from: "studio" as const, to, label, active: alive(to) })),
        ...SERVICE_EDGES.map(([from, to, label]) => ({ from, to, label, active: alive(from) && alive(to) })),
    ];
    return { nodes, edges };
}
