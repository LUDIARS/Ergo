// @implements SPEC-ergo-studio-guard
/// Studio の変更系 API を、Ergo 自身の画面からの呼び出しに限る (純関数)。
///
/// Ergo は 127.0.0.1 で待ち受けるが、ブラウザで開いた別サイトが localhost へ POST すると
/// AI への実装指示やプラグイン導入を勝手に起こせてしまう。そこで変更系は
///   - Host が loopback であること (DNS rebinding 対策)
///   - Origin を付けるなら Ergo 自身 (同じ host:port) であること
///   - JSON で送ること (別サイトからは事前確認が要り、CORS を返さないので通らない)
/// を満たすときだけ受け付ける。Origin の無い呼び出し (curl やテスト) は Host 条件だけで判定する。

const LOOPBACK_HOSTS = new Set(["localhost", "127.0.0.1", "[::1]", "::1"]);

function hostOnly(hostPort: string): string {
    const v = hostPort.trim().toLowerCase();
    if (v.startsWith("[")) return v.slice(0, v.indexOf("]") + 1);
    const i = v.lastIndexOf(":");
    return i >= 0 ? v.slice(0, i) : v;
}

export type GuardDecision = { ok: true } | { ok: false; reason: string };

export function checkMutation(input: { method: string; host: string | undefined;
                                       origin: string | undefined; contentType: string | undefined }): GuardDecision {
    const method = input.method.toUpperCase();
    if (method === "GET" || method === "HEAD" || method === "OPTIONS") return { ok: true };
    if (!input.host || !LOOPBACK_HOSTS.has(hostOnly(input.host))) return { ok: false, reason: "loopback 以外の Host" };
    if (input.origin !== undefined) {
        let origin: URL;
        try {
            origin = new URL(input.origin);
        } catch {
            return { ok: false, reason: "Origin が不正" };
        }
        if (!LOOPBACK_HOSTS.has(origin.hostname.toLowerCase()) && !LOOPBACK_HOSTS.has(`[${origin.hostname}]`)) {
            return { ok: false, reason: "別サイトからの呼び出し" };
        }
        const port = origin.port || (origin.protocol === "https:" ? "443" : "80");
        const hostPort = input.host.slice(hostOnly(input.host).length + 1) || "80";
        if (port !== hostPort) return { ok: false, reason: "別ポートからの呼び出し" };
    }
    if (method !== "DELETE" && !(input.contentType ?? "").toLowerCase().startsWith("application/json")) {
        return { ok: false, reason: "JSON 以外の送信" };
    }
    return { ok: true };
}
