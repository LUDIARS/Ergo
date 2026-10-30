// @implements SPEC-ergo-studio-server-side
// @implements SPEC-ergo-studio-secrets
/// サービス呼び出しの結果を Studio の HTTP 応答へ写す共通処理。

import type { Context } from "hono";

import type { FailureKind, ServiceResult } from "../net/http.js";

const STATUS: Record<FailureKind, 400 | 401 | 403 | 404 | 502 | 503 | 504> = {
    not_configured: 503,
    unreachable:    503,
    timeout:        504,
    unauthorized:   401,
    forbidden:      403,
    not_found:      404,
    rejected:       400,
    server_error:   502,
    bad_response:   502,
};

export function sendResult<T>(c: Context, service: string, r: ServiceResult<T>, wrap: (data: T) => unknown) {
    if (r.ok) return c.json(wrap(r.data) as object);
    return c.json({ error: r.kind, service, message: r.message }, STATUS[r.kind]);
}

export function sendError(c: Context, status: 400 | 404 | 409 | 500 | 502 | 503, message: string) {
    return c.json({ error: "studio", message }, status);
}

/// JSON 本文を読む。壊れていれば null。
export async function readJson(c: Context): Promise<Record<string, unknown> | null> {
    const body = await c.req.json().catch(() => null);
    return body && typeof body === "object" && !Array.isArray(body) ? (body as Record<string, unknown>) : null;
}

export function str(v: unknown): string | undefined {
    return typeof v === "string" ? v : undefined;
}
