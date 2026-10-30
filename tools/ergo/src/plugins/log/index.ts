/// log plugin — live ergo_log viewer: per-app line rings + WS relay.
///
/// Engine clients (in-repo: `ergo_log_live`, include/ergo/log/live_sink.h)
/// send { op: "hello", role: "engine", app } and then batches of
/// { op: "lines", app, dropped, lines }. Each app keeps its newest 10k lines
/// in a ring (also after its engine disconnects, so the log of a crashed run
/// stays readable). UI clients get every ring as `backlog` on connect, then
/// live `lines`. Protocol: spec/feature/tool/log.md.

import { Hono } from "hono";
import { WebSocket as WS } from "ws";

import type { Plugin, PluginContext, PluginFactory } from "../../core/plugin.js";
import { isLevelTag, sanitizeLinesMsg } from "./protocol.js";
import type { AppInfo, LevelTag, LogLine, ServerToUi } from "./protocol.js";
import { LineRing, RING_CAPACITY } from "./ring.js";
import type { LineFilter } from "./ring.js";

/** Rings kept at most. Beyond it, the least recently active app whose engine
 *  has disconnected is evicted (apps with a live engine are never evicted). */
export const MAX_APPS = 16;
/** Live engine connections accepted at once. A further engine is closed with
 *  1013 (ergo_log_live reconnects later), so the rings never exceed MAX_APPS. */
export const MAX_ENGINES = MAX_APPS;
/** Default `limit` of GET /api/lines (the maximum is the ring capacity). */
export const DEFAULT_LINES_LIMIT = 1000;

interface ConnState {
    ws:   WS;
    role: "engine" | "ui" | "unknown";
    app:  AppState | null;   // engines only
}

interface AppState {
    name:       string;
    ring:       LineRing;
    /** Engine connection feeding this app; null once it disconnected. */
    owner:      ConnState | null;
    /** Sum of the `dropped` counts the engine reported. */
    dropped:    number;
    /** Activity tick, for eviction order. */
    lastActive: number;
}

type Query = Record<string, string | undefined>;

/** Parse GET /api/lines query parameters into a filter + limit, or an error. */
function parseLinesQuery(q: Query): { filter: LineFilter; limit: number } | { error: string } {
    const filter: LineFilter = {};
    if (q.level) {
        const levels = new Set<LevelTag>();
        for (const part of q.level.split(",")) {
            const tag = part.trim().toUpperCase();
            if (!isLevelTag(tag)) return { error: `unknown level "${part}" (use ERR,WRN,INF,DBG)` };
            levels.add(tag);
        }
        filter.levels = levels;
    }
    const numbers: Array<[string, "since" | "frameMin" | "frameMax"]> = [
        ["since", "since"], ["frame_min", "frameMin"], ["frame_max", "frameMax"],
    ];
    for (const [param, key] of numbers) {
        const raw = q[param];
        if (raw === undefined || raw === "") continue;
        const n = Number(raw);
        if (!Number.isFinite(n)) return { error: `${param} must be a number` };
        filter[key] = n;
    }
    if (q.q) filter.q = q.q;

    let limit = DEFAULT_LINES_LIMIT;
    if (q.limit !== undefined && q.limit !== "") {
        const n = Number(q.limit);
        if (!Number.isInteger(n) || n < 1) return { error: "limit must be a positive integer" };
        limit = Math.min(n, RING_CAPACITY);
    }
    return { filter, limit };
}

const factory: PluginFactory = () => {
    const conns = new Set<ConnState>();
    const apps  = new Map<string, AppState>();   // insertion order = first seen
    let tick = 0;

    function send(ws: WS, msg: ServerToUi) {
        if (ws.readyState === WS.OPEN) ws.send(JSON.stringify(msg));
    }

    function broadcastToUis(msg: ServerToUi) {
        const text = JSON.stringify(msg);
        for (const c of conns) {
            if (c.role === "ui" && c.ws.readyState === WS.OPEN) c.ws.send(text);
        }
    }

    function appInfo(a: AppState): AppInfo {
        return {
            app:       a.name,
            connected: a.owner !== null,
            lines:     a.ring.size,
            last_seq:  a.ring.lastSeq,
            dropped:   a.dropped,
        };
    }

    function appList(): AppInfo[] {
        return [...apps.values()].map(appInfo);
    }

    function evictIfNeeded() {
        while (apps.size > MAX_APPS) {
            let victim: AppState | null = null;
            for (const a of apps.values()) {
                if (a.owner === null && (victim === null || a.lastActive < victim.lastActive)) victim = a;
            }
            if (!victim) return;   // every app has a live engine
            apps.delete(victim.name);
        }
    }

    /// Bind `conn` to an app named `requested`. A name held by another live
    /// engine gets a suffix (`game#2`, `game#3`, ...); a name whose engine
    /// disconnected is taken over, so a restarted engine continues its ring.
    function claimApp(requested: string, conn: ConnState): AppState {
        const base = requested.trim() || "anonymous";
        for (let n = 1; ; ++n) {
            const name     = n === 1 ? base : `${base}#${n}`;
            const existing = apps.get(name);
            if (existing && existing.owner !== null) continue;
            const app = existing ?? {
                name, ring: new LineRing(RING_CAPACITY), owner: null, dropped: 0, lastActive: 0,
            };
            app.owner      = conn;
            app.lastActive = ++tick;
            if (!existing) {
                apps.set(name, app);
                evictIfNeeded();
            }
            return app;
        }
    }

    function engineCount(): number {
        let n = 0;
        for (const c of conns) if (c.role === "engine") ++n;
        return n;
    }

    function becomeEngine(conn: ConnState, requested: string, ctx: PluginContext) {
        if (engineCount() >= MAX_ENGINES) {
            if (conn.ws.readyState === WS.OPEN) {
                ctx.log("warn", `[log] engine refused: ${MAX_ENGINES} engines already connected`);
                conn.ws.close(1013, "too many engines");
            }
            return;
        }
        conn.role = "engine";
        conn.app  = claimApp(requested, conn);
        ctx.log("info", `[log] engine connected: ${conn.app.name}`);
        broadcastToUis({ op: "apps", apps: appList() });
    }

    function ingest(app: AppState, dropped: number, lines: LogLine[]) {
        // A batch larger than the ring only keeps its tail.
        const stored = lines.slice(-app.ring.capacity).map((l) => app.ring.push(l));
        app.dropped   += dropped;
        app.lastActive = ++tick;
        if (stored.length > 0 || dropped > 0) {
            broadcastToUis({ op: "lines", app: app.name, dropped, lines: stored });
        }
    }

    function sendSnapshot(ws: WS) {
        send(ws, { op: "apps", apps: appList() });
        for (const a of apps.values()) {
            send(ws, { op: "backlog", app: a.name, dropped: a.dropped, lines: a.ring.query() });
        }
    }

    const plugin: Plugin = {
        id:          "log",
        title:       "Log",
        icon:        "📜",
        description: "Live ergo_log viewer · per-app ring buffers · level / text / frame filters",
        staticRoot:  "./src/plugins/log/ui",

        routes(_ctx: PluginContext) {
            const app = new Hono();
            app.get("/api/health", (c) => c.json(plugin.health!()));
            app.get("/api/apps", (c) => c.json({ apps: appList() }));
            app.get("/api/lines", (c) => {
                const q = c.req.query() as Query;
                if (!q.app) {
                    return c.json({ error: "app is required", apps: [...apps.keys()] }, 400);
                }
                const target = apps.get(q.app);
                if (!target) return c.json({ error: `unknown app "${q.app}"`, apps: [...apps.keys()] }, 404);
                const parsed = parseLinesQuery(q);
                if ("error" in parsed) return c.json({ error: parsed.error }, 400);
                return c.json({
                    app:      target.name,
                    last_seq: target.ring.lastSeq,
                    dropped:  target.dropped,
                    lines:    target.ring.query(parsed.filter, parsed.limit),
                });
            });
            return app;
        },

        onUpgrade(_req, ws: WS, ctx: PluginContext) {
            const state: ConnState = { ws, role: "unknown", app: null };
            conns.add(state);

            ws.on("message", (raw: unknown) => {
                let parsed: unknown;
                try { parsed = JSON.parse(String(raw)); } catch { return; }
                if (!parsed || typeof parsed !== "object") return;
                const msg = parsed as Record<string, unknown>;

                if (msg.op === "hello") {
                    if (msg.role === "engine" && state.role === "unknown") {
                        becomeEngine(state, typeof msg.app === "string" ? msg.app : "", ctx);
                    } else if (msg.role === "ui" && state.role !== "engine") {
                        state.role = "ui";
                        sendSnapshot(ws);
                    }
                    return;
                }

                if (msg.op === "lines") {
                    const body = sanitizeLinesMsg(msg);
                    if (!body) return;
                    // After a reconnect the first batch can overtake `hello`:
                    // the frame's own `app` names the engine then.
                    if (state.role === "unknown") becomeEngine(state, body.app ?? "", ctx);
                    if (state.role !== "engine" || !state.app) return;
                    ingest(state.app, body.dropped, body.lines);
                }
            });

            ws.on("close", () => {
                conns.delete(state);
                if (state.role === "engine" && state.app && state.app.owner === state) {
                    state.app.owner      = null;
                    state.app.lastActive = ++tick;
                    ctx.log("info", `[log] engine disconnected: ${state.app.name}`);
                    broadcastToUis({ op: "apps", apps: appList() });
                }
            });
            ws.on("error", () => { /* close handler tidies up */ });
        },

        health() {
            let engines = 0;
            let uis     = 0;
            for (const c of conns) {
                if (c.role === "engine") ++engines;
                else if (c.role === "ui") ++uis;
            }
            return { ok: true, apps: apps.size, engines, uis };
        },
    };

    return plugin;
};

export default factory;
