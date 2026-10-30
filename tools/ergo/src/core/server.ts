/// Shared HTTP + WS bootstrap.
///
/// Creates one Hono app, mounts the shell UI at `/` and each plugin at
/// `/${id}/*`. WS upgrades are routed by the leading path segment:
///   /${id}/ws -> plugin[id].onUpgrade(...)
///
/// Startup is split into three testable steps that `boot()` composes:
///   instantiatePlugins() — call every factory, validate the returned shape
///                          and the id (built-in failures throw, external
///                          pack entries are logged, skipped and recorded);
///   createApp()          — assemble the Hono app (shell, /api/*, plugins);
///   attachWs()           — route WS upgrades on an HTTP server by plugin id.

import { Hono } from "hono";
import { serve, type ServerType } from "@hono/node-server";
import { serveStatic } from "@hono/node-server/serve-static";
import type { IncomingMessage } from "node:http";
import type { Duplex } from "node:stream";
import { WebSocketServer } from "ws";

import type { ExternalFactory } from "./external.js";
import type { Plugin, PluginContext, PluginFactory, PluginHealth } from "./plugin.js";
import { crossSiteProblem, wsHandshakeProblem } from "./request_guard.js";

export interface BootOptions {
    port:      number;
    /** Built-in factories (plain functions, a failure aborts startup) and
     *  external pack entries from `loadExternalFactories()` (`{ factory,
     *  source }`, a failure is logged and the entry skipped). */
    factories: Array<PluginFactory | ExternalFactory>;
    /** Bind address. Defaults to "127.0.0.1" to avoid LAN exposure. */
    hostname?: string;
}

/** A plugin instance together with where it came from. */
export interface LoadedPlugin {
    plugin:  Plugin;
    /** "built-in" for registry.ts entries, otherwise the pack entry path. */
    source:  string;
    /** Built-in failures abort startup; external ones are skipped. */
    builtin: boolean;
}

/** An external pack entry that was skipped. Reported by /api/health. */
export interface RejectedPlugin {
    /** The id the entry claimed, or "" when it never produced one. */
    id:     string;
    source: string;
    reason: string;
}

/** Path-safe plugin id: it becomes a URL segment and a route prefix. */
export const PLUGIN_ID_PATTERN = /^[a-z0-9][a-z0-9_-]*$/;

/** Ids taken by the shared server's own routes (/api/*, /shell/*). */
export const RESERVED_PLUGIN_IDS: ReadonlySet<string> = new Set(["api", "shell"]);

const BUILTIN_SOURCE = "built-in";

/** Default context: tagged console logging. */
export const consoleContext: PluginContext = {
    log(level, msg) {
        const tag = level === "info" ? "[info]" : level === "warn" ? "[warn]" : "[err]";
        const fn  = level === "error" ? console.error : level === "warn" ? console.warn : console.log;
        fn(`${tag} ${msg}`);
    },
};

function errorMessage(err: unknown): string {
    return err instanceof Error ? err.message : String(err);
}

/// Duck-type the object a factory returned. Returns the reason it cannot
/// be mounted, or null when it satisfies the Plugin contract.
function shapeProblem(p: unknown): string | null {
    if (!p || typeof p !== "object") return "factory did not return an object";
    const o = p as Record<string, unknown>;
    if (typeof o.id !== "string") return "id is not a string";
    if (!PLUGIN_ID_PATTERN.test(o.id)) {
        return `invalid id '${o.id}' (must match ${PLUGIN_ID_PATTERN})`;
    }
    if (RESERVED_PLUGIN_IDS.has(o.id)) return `reserved id '${o.id}'`;
    if (typeof o.title !== "string") return "title is not a string";
    for (const key of ["description", "icon", "staticRoot"]) {
        if (o[key] !== undefined && typeof o[key] !== "string") return `${key} is not a string`;
    }
    for (const key of ["routes", "onUpgrade", "health"]) {
        if (o[key] !== undefined && typeof o[key] !== "function") return `${key} is not a function`;
    }
    return null;
}

/// Call one factory and check the result against the shape and id rules.
/// `owner` maps the ids taken so far to their source.
function tryInstantiate(
    factory: PluginFactory,
    owner:   ReadonlyMap<string, string>,
): { plugin: Plugin } | { id: string; reason: string } {
    let id = "";
    try {
        const p = factory();
        const o = p as unknown as Record<string, unknown> | null;
        if (o && typeof o === "object" && typeof o.id === "string") id = o.id;
        const problem = shapeProblem(p);
        if (problem !== null) return { id, reason: problem };
        const prev = owner.get(id);
        if (prev !== undefined) return { id, reason: `duplicate id '${id}' (already provided by ${prev})` };
        return { plugin: p };
    } catch (err) {
        return { id, reason: `factory threw: ${errorMessage(err)}` };
    }
}

/// Call every factory once and validate the result.
///
/// Built-ins come first. A built-in that throws, has an invalid shape or
/// reuses an id throws (fail fast: it is a bug in this repo). An external
/// entry with the same problems is logged, skipped and recorded in
/// `rejected` — a broken pack never aborts startup. The first plugin to
/// claim an id wins.
export function instantiatePlugins(
    builtins:  PluginFactory[],
    externals: ExternalFactory[],
    log:       PluginContext["log"] = consoleContext.log,
): { plugins: LoadedPlugin[]; rejected: RejectedPlugin[] } {
    const plugins:  LoadedPlugin[]   = [];
    const rejected: RejectedPlugin[] = [];
    const owner = new Map<string, string>();   // id -> source

    const entries = [
        ...builtins.map((factory) => ({ factory, source: BUILTIN_SOURCE, builtin: true })),
        ...externals.map((e) => ({ factory: e.factory, source: e.source, builtin: false })),
    ];
    for (const { factory, source, builtin } of entries) {
        const r = tryInstantiate(factory, owner);
        if ("plugin" in r) {
            owner.set(r.plugin.id, source);
            plugins.push({ plugin: r.plugin, source, builtin });
            continue;
        }
        if (builtin) throw new Error(`[ergo] built-in plugin${r.id ? ` '${r.id}'` : ""} rejected: ${r.reason}`);
        log("error", `[ergo] external plugin ${source} rejected: ${r.reason}`);
        rejected.push({ id: r.id, source, reason: r.reason });
    }
    return { plugins, rejected };
}

/// Normalise one plugin's health() for /api/health: a throw or a result
/// without a boolean `ok` is reported as ok:false instead of failing the
/// whole response.
function pluginHealth(p: Plugin): PluginHealth & { id: string; title: string } {
    let h: PluginHealth;
    try {
        const r: unknown = p.health ? p.health() : { ok: true };
        if (!r || typeof r !== "object" || typeof (r as PluginHealth).ok !== "boolean") {
            h = { ok: false, err: "health() must return { ok: boolean }" };
        } else {
            h = r as PluginHealth;
        }
    } catch (err) {
        h = { ok: false, err: `health() threw: ${errorMessage(err)}` };
    }
    return { ...h, id: p.id, title: p.title };
}

/// Assemble the shared Hono app: shell UI, /api/health, /api/plugins and
/// every plugin's static UI + routes. A plugin whose routes(ctx) throws is
/// handled like a factory failure (built-in: throw, external: skip and add
/// to `rejected`). Returns the mounted plugins for attachWs().
/// Every request first passes the cross-site / DNS-rebinding guard
/// (request_guard.ts): `bindHost` is the ERGO_HOST name, if any.
export function createApp(
    loaded:    LoadedPlugin[],
    rejected:  RejectedPlugin[],
    ctx:       PluginContext,
    bindHost?: string,
): { app: Hono; plugins: Plugin[]; rejected: RejectedPlugin[] } {
    const app = new Hono();
    // Before any route: a web page must not read or write through the tool.
    app.use("*", async (c, next) => {
        const problem = crossSiteProblem(new URL(c.req.url).host, c.req.header("origin"), bindHost);
        if (problem !== null) return c.json({ ok: false, err: problem }, 403);
        await next();
    });
    const plugins: Plugin[] = [];
    const skipped: RejectedPlugin[] = [...rejected];
    const subApps = new Map<string, Hono>();

    for (const { plugin: p, source, builtin } of loaded) {
        if (p.routes) {
            try {
                subApps.set(p.id, p.routes(ctx));
            } catch (err) {
                const reason = `routes() threw: ${errorMessage(err)}`;
                if (builtin) throw new Error(`[ergo] built-in plugin '${p.id}' rejected: ${reason}`);
                ctx.log("error", `[ergo] external plugin ${source} rejected: ${reason}`);
                skipped.push({ id: p.id, source, reason });
                continue;
            }
        }
        plugins.push(p);
    }

    // Shell UI — served from ./public, reachable under /shell/*.
    // rewriteRequestPath strips the /shell prefix so "/shell/index.html"
    // maps to "./public/index.html".
    app.use("/shell/*", serveStatic({
        root:               "./public",
        rewriteRequestPath: (p) => p.replace(/^\/shell/, ""),
    }));
    app.get("/", (c) => c.redirect("/shell/index.html"));
    app.get("/shell", (c) => c.redirect("/shell/index.html"));

    // Global health — aggregates every plugin's health(). Skipped external
    // entries are listed under `rejected` (not `plugins`, so a duplicate id
    // never repaints the badge of the plugin that won it).
    app.get("/api/health", (c) => {
        const list = plugins.map(pluginHealth);
        return c.json({
            ok:       list.every((p) => p.ok) && skipped.length === 0,
            plugins:  list,
            rejected: skipped.map((r) => ({ id: r.id, source: r.source, ok: false, err: r.reason })),
        });
    });

    // List plugins (used by the shell UI for its sidebar).
    app.get("/api/plugins", (c) =>
        c.json({
            plugins: plugins.map((p) => ({
                id:          p.id,
                title:       p.title,
                description: p.description ?? "",
                icon:        p.icon        ?? "",
                url:         `/${p.id}/`,
                ws:          `/${p.id}/ws`,
            })),
        })
    );

    // Mount every plugin's static UI + API routes.
    for (const p of plugins) {
        if (p.staticRoot) {
            // Serve the plugin's browser UI from its own directory. The
            // prefix strips "/${p.id}" before resolving under `staticRoot`.
            app.use(
                `/${p.id}/*`,
                serveStatic({
                    root: p.staticRoot,
                    rewriteRequestPath: (path) =>
                        path.replace(new RegExp(`^/${p.id}`), ""),
                }),
            );
            // Redirect bare /${p.id} (no trailing slash) to /${p.id}/.
            app.get(`/${p.id}`, (c) => c.redirect(`/${p.id}/`));
        }
        const sub = subApps.get(p.id);
        if (sub) app.route(`/${p.id}`, sub);
    }

    return { app, plugins, rejected: skipped };
}

/// Route WebSocket upgrades on `server` by URL before accepting: only
/// /${id}/ws of a plugin with onUpgrade is accepted, anything else (an
/// unparsable request target included) has its socket destroyed. A
/// handshake from another site (an Origin that is not the tool's own) is
/// answered 403. One WebSocketServer serves every plugin. A plugin whose
/// onUpgrade throws is logged and that connection closed with 1011; the
/// server keeps running. (Errors thrown later from the plugin's own socket
/// listeners are not contained: plugins run in-process.)
export function attachWs(
    server:    ServerType,
    plugins:   Plugin[],
    ctx:       PluginContext,
    bindHost?: string,
): WebSocketServer {
    const byId = new Map<string, Plugin>();
    for (const p of plugins) if (p.onUpgrade) byId.set(p.id, p);

    const wss = new WebSocketServer({ noServer: true });

    server.on("upgrade", (req: IncomingMessage, socket: Duplex, head: Buffer) => {
        // `req.url` is the raw request target. llhttp accepts some targets the
        // WHATWG URL parser rejects (e.g. "//[/ws", "http://a:99999/"), and a
        // throw in this listener is uncaught and would kill the process.
        let pathname: string;
        try {
            pathname = new URL(req.url ?? "/", "http://localhost").pathname;
        } catch {
            socket.destroy();
            return;
        }
        // Accept only /${id}/ws style paths.
        const m = pathname.match(/^\/([^/]+)\/ws\/?$/);
        const plugin = m ? byId.get(m[1]) : undefined;
        if (!plugin) {
            socket.destroy();
            return;
        }
        // A browser does not apply same-origin rules to WebSocket: refuse a
        // handshake from another site (native engine clients send no Origin).
        const problem = wsHandshakeProblem(req.headers.host, req.headers.origin, bindHost);
        if (problem !== null) {
            ctx.log("warn", `[ergo] refused WS ${pathname}: ${problem}`);
            socket.end("HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n");
            return;
        }
        wss.handleUpgrade(req, socket, head, (ws) => {
            // This runs synchronously inside the http "upgrade" listener, so
            // a throw would escape as an uncaught exception and stop the
            // whole server. Contain it to this one connection.
            try {
                plugin.onUpgrade!(req, ws, ctx);
            } catch (err) {
                ctx.log("error", `[ergo] plugin '${plugin.id}' onUpgrade threw: ${errorMessage(err)}`);
                ws.close(1011, "plugin error");
            }
        });
    });

    return wss;
}

export function boot(opts: BootOptions): void {
    const ctx = consoleContext;
    const builtins  = opts.factories.filter((f): f is PluginFactory   => typeof f === "function");
    const externals = opts.factories.filter((f): f is ExternalFactory => typeof f !== "function");

    // Instantiate every plugin once so we can attach state / WS handlers.
    const loaded = instantiatePlugins(builtins, externals, ctx.log);
    const { app, plugins, rejected } = createApp(loaded.plugins, loaded.rejected, ctx, opts.hostname);

    // ---- HTTP + WS server (shared port) ----
    // Let @hono/node-server create + listen the HTTP server; we attach
    // the WebSocket upgrade routing to the returned instance.
    const httpServer = serve({ fetch: app.fetch, port: opts.port, hostname: opts.hostname ?? "127.0.0.1" }, () => {
        console.log(`[ergo] http://localhost:${opts.port}/`);
        for (const p of plugins) {
            console.log(`[ergo]   -> ${p.title}:  http://localhost:${opts.port}/${p.id}/  (ws: /${p.id}/ws)`);
        }
        for (const r of rejected) {
            console.error(`[ergo]   !! rejected ${r.source}: ${r.reason}`);
        }
    });

    attachWs(httpServer, plugins, ctx, opts.hostname);
}
