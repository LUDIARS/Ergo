// @implements SPEC-ergo-studio-server-side
// @implements SPEC-ergo-studio-secrets
/// サービス呼び出しの共通口。結果を「成功 / 失敗の種類」に分けて返し、
/// 例外を画面まで漏らさない。失敗の本文はそのまま UI に出さない (先頭だけを残す)。

export type FailureKind =
    | "not_configured"   // 接続先が決まらない
    | "unreachable"      // 接続拒否・名前解決失敗
    | "timeout"
    | "unauthorized"     // 401
    | "forbidden"        // 403
    | "not_found"        // 404
    | "rejected"         // その他 4xx (入力不正など)
    | "server_error"     // 5xx
    | "bad_response";    // JSON でない等

export type ServiceResult<T> =
    | { ok: true; data: T; status: number }
    | { ok: false; kind: FailureKind; status?: number; message: string };

export function classifyStatus(status: number): FailureKind {
    if (status === 401) return "unauthorized";
    if (status === 403) return "forbidden";
    if (status === 404) return "not_found";
    if (status >= 500) return "server_error";
    return "rejected";
}

/// 失敗本文の要約。サービスの `{error}` を優先し、長さを抑える。
export function summarizeError(body: string): string {
    try {
        const o = JSON.parse(body) as { error?: unknown; message?: unknown };
        const msg = typeof o.error === "string" ? o.error : typeof o.message === "string" ? o.message : null;
        if (msg) return msg.slice(0, 300);
    } catch { /* JSON でない本文 */ }
    return body.replace(/\s+/g, " ").trim().slice(0, 300);
}

export interface RequestOptions {
    method?:  "GET" | "POST" | "PUT" | "PATCH" | "DELETE";
    body?:    unknown;
    headers?: Record<string, string>;
    timeoutMs: number;
}

export async function requestJson<T>(base: string | null, path: string, opts: RequestOptions): Promise<ServiceResult<T>> {
    if (!base) return { ok: false, kind: "not_configured", message: "接続先が未設定です" };
    const headers: Record<string, string> = { accept: "application/json", ...(opts.headers ?? {}) };
    let body: string | undefined;
    if (opts.body !== undefined) {
        headers["content-type"] = "application/json; charset=utf-8";
        body = JSON.stringify(opts.body);
    }
    let res: Response;
    try {
        res = await fetch(base + path, {
            method: opts.method ?? "GET",
            headers,
            body,
            signal: AbortSignal.timeout(opts.timeoutMs),
        });
    } catch (err) {
        const name = (err as Error)?.name;
        if (name === "TimeoutError" || name === "AbortError") {
            return { ok: false, kind: "timeout", message: `${opts.timeoutMs}ms で応答がありません` };
        }
        return { ok: false, kind: "unreachable", message: (err as Error)?.message ?? "接続できません" };
    }
    const text = await res.text();
    if (!res.ok) {
        return { ok: false, kind: classifyStatus(res.status), status: res.status, message: summarizeError(text) };
    }
    if (text.trim() === "") return { ok: true, data: undefined as T, status: res.status };
    try {
        return { ok: true, data: JSON.parse(text) as T, status: res.status };
    } catch {
        return { ok: false, kind: "bad_response", status: res.status, message: "JSON ではない応答です" };
    }
}
