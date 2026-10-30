/// Wire protocol for the `log` plugin <-> `ergo_log_live` engine client.
///
/// Engines (C++: `ergo::log::LiveSink`, include/ergo/log/live_sink.h) send
/// `hello` on every (re)connect and then batches of `lines`; the plugin keeps
/// a ring of recent lines per app and fans new lines out to UI clients.
/// Spec: spec/feature/tool/log.md.
///
/// Besides the types, this file holds the pure guards the relay uses on
/// untrusted frames, and `parseConsoleLine()` for ergo_log's console format,
/// so a process runner can feed raw stdout into the same model.

/// ergo_log level tags, most severe first (mirrors `ergo::log::Level`).
export type LevelTag = "ERR" | "WRN" | "INF" | "DBG";
export const LEVEL_TAGS: readonly LevelTag[] = ["ERR", "WRN", "INF", "DBG"];

export function isLevelTag(v: unknown): v is LevelTag {
    return typeof v === "string" && (LEVEL_TAGS as readonly string[]).includes(v);
}

/// One log record as the engine sends it.
export interface LogLine {
    lvl:   LevelTag;
    /** Frame number from ergo_log's frame provider (0 when not wired). */
    frame: number;
    /** Engine time in µs from ergo_log's time provider (`ergo::profile::now_us` when wired). */
    ts_us: number;
    /** Emitting thread (hashed std::thread::id, same as ergo_profile's tid). */
    tid:   number;
    msg:   string;
}

/// A line as stored by the plugin: `seq` numbers the lines of one app
/// (1, 2, 3, ...) and never repeats, also across engine reconnects.
export interface StoredLine extends LogLine {
    seq: number;
}

export interface AppInfo {
    /** Display name, unique in the plugin (`game`, `game#2`, ...). */
    app:       string;
    /** An engine connection currently feeds this app. */
    connected: boolean;
    /** Lines held in the ring. */
    lines:     number;
    /** seq of the newest line (0 before the first). */
    last_seq:  number;
    /** Lines the engine reported as dropped before they reached the plugin. */
    dropped:   number;
}

// ---- Engine -> Server ------------------------------------------------------
/**
 * `hello` names the engine. A `lines` frame that arrives before `hello`
 * (possible right after a reconnect) registers the connection as an engine
 * named by the frame's `app`. `dropped` counts the records the engine
 * discarded since its previous frame.
 */
export type EngineMsg =
    | { op: "hello"; role: "engine"; app?: string }
    | { op: "lines"; app?: string; dropped?: number; lines: LogLine[] };

// ---- UI -> Server ----------------------------------------------------------
export type UiMsg =
    | { op: "hello"; role: "ui" };

// ---- Server -> UI ----------------------------------------------------------
/**
 * After `hello`, a UI receives `apps` and one `backlog` per app (the whole
 * ring), then live `lines` and an `apps` update whenever an engine
 * connects or disconnects.
 */
export type ServerToUi =
    | { op: "apps";    apps: AppInfo[] }
    | { op: "backlog"; app: string; dropped: number; lines: StoredLine[] }
    | { op: "lines";   app: string; dropped: number; lines: StoredLine[] };

// ---- Guards ------------------------------------------------------------------

function finiteNumber(v: unknown): number | null {
    return typeof v === "number" && Number.isFinite(v) ? v : null;
}

/// Longest `msg` kept, in UTF-16 code units. Mirrors the engine's
/// `LiveSink::kMaxMessageBytes` (8192), which other clients need not honour:
/// without it one ring could hold 10k arbitrarily long lines.
export const MAX_MSG_CHARS = 8192;

/// Cut `msg` to MAX_MSG_CHARS (not inside a surrogate pair) with a trailing
/// "…", the way `ergo::log::LiveSink` cuts on the engine side.
export function clipMessage(msg: string): string {
    if (msg.length <= MAX_MSG_CHARS) return msg;
    let cut = MAX_MSG_CHARS;
    const last = msg.charCodeAt(cut - 1);
    if (last >= 0xd800 && last <= 0xdbff) --cut;
    return msg.slice(0, cut) + "\u2026";
}

/// Validate one engine line. Returns null for anything that is not a
/// well-formed record (unknown level, non-numeric frame, non-string msg...).
/// A `msg` longer than MAX_MSG_CHARS is cut (clipMessage).
export function sanitizeLine(raw: unknown): LogLine | null {
    if (!raw || typeof raw !== "object") return null;
    const r = raw as Record<string, unknown>;
    if (!isLevelTag(r.lvl) || typeof r.msg !== "string") return null;
    const frame = finiteNumber(r.frame);
    const ts_us = finiteNumber(r.ts_us);
    const tid   = finiteNumber(r.tid);
    if (frame === null || ts_us === null || tid === null) return null;
    return { lvl: r.lvl, frame, ts_us, tid, msg: clipMessage(r.msg) };
}

/// Validate a `lines` frame body. Malformed lines are skipped; returns null
/// when `lines` is not an array. `dropped` defaults to 0.
export function sanitizeLinesMsg(raw: Record<string, unknown>): { app: string | null; dropped: number; lines: LogLine[] } | null {
    if (!Array.isArray(raw.lines)) return null;
    const lines: LogLine[] = [];
    for (const l of raw.lines) {
        const line = sanitizeLine(l);
        if (line) lines.push(line);
    }
    const dropped = finiteNumber(raw.dropped);
    return {
        app:     typeof raw.app === "string" && raw.app !== "" ? raw.app : null,
        dropped: dropped !== null && dropped > 0 ? Math.floor(dropped) : 0,
        lines,
    };
}

// ---- Console format ------------------------------------------------------------

/// What ergo_log's console sink prints: `[F%08llu][TAG] msg`.
export interface ConsoleLine {
    lvl:   LevelTag;
    frame: number;
    msg:   string;
}

const CONSOLE_RE = /^\[F(\d+)\]\[(ERR|WRN|INF|DBG)\](?: (.*))?$/s;

/// Parse one line of ergo_log console output (a trailing `\n` / `\r\n` /
/// `\r` is ignored). Returns null for lines in any other format.
export function parseConsoleLine(text: string): ConsoleLine | null {
    const m = CONSOLE_RE.exec(text.replace(/\r?\n?$/, ""));
    if (!m) return null;
    return { frame: Number(m[1]), lvl: m[2] as LevelTag, msg: m[3] ?? "" };
}
