/// Shared HTTP + WS bootstrap.
///
/// Creates one Hono app, mounts the shell UI at `/` and each plugin at
/// `/${id}/*`. WS upgrades are routed by the leading path segment:
///   /${id}/ws -> plugin[id].onUpgrade(...)

import { Hono } from "hono";
import { serve } from "@hono/node-server";
import { serveStatic } from "@hono/node-server/serve-static";
import type { IncomingMessage } from "node:http";
import type { Duplex } from "node:stream";
import { WebSocketServer } from "ws";

import type { Plugin, PluginContext, PluginFactory } from "./plugin.js";

export interface BootOptions {
    port:      number;
    factories: PluginFactory[];
    /** Bind address. Defaults to "127.0.0.1" to avoid LAN exposure. */
    hostname?: string;
}

export function boot(opts: BootOptions): void {
    // Instantiate every plugin once so we can attach state / WS handlers.
    const plugins: Plugin[] = opts.factories.map((f) => f());

    // ---- Shared HTTP app ----
    const app = new Hono();

    // Shell UI — served from ./public, reachable under /shell/*.
    // rewriteRequestPath strips the /shell prefix so "/shell/index.html"
    // maps to "./public/index.html".
    app.use("/shell/*", serveStatic({
        root:               "./public",
        rewriteRequestPath: (p) => p.replace(/^\/shell/, ""),
    }));
    app.get("/", (c) => c.redirect("/shell/index.html"));
    app.get("/shell", (c) => c.redirect("/shell/index.html"));

    // Global health — aggregates every plugin's health().
    app.get("/api/health", (c) =>
        c.json({
            ok: true,
            plugins: plugins.map((p) => ({
                id:    p.id,
                title: p.title,
                ...(p.health?.() ?? { ok: true }),
            })),
        })
    );

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

    const ctx: PluginContext = {
        log(level, msg) {
            const tag = level === "info" ? "[info]" : level === "warn" ? "[warn]" : "[err]";
            const fn  = level === "error" ? console.error : level === "warn" ? console.warn : console.log;
            fn(`${tag} ${msg}`);
        },
    };

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
        if (p.routes) {
            app.route(`/${p.id}`, p.routes(ctx));
        }
    }

    // ---- HTTP + WS server (shared port) ----
    // Let @hono/node-server create + listen the HTTP server; we attach
    // the WebSocket upgrade routing to the returned instance.
    const httpServer = serve({ fetch: app.fetch, port: opts.port, hostname: opts.hostname ?? "127.0.0.1" }, () => {
        console.log(`[ergo] http://localhost:${opts.port}/`);
        for (const p of plugins) {
            console.log(`[ergo]   -> ${p.title}:  http://localhost:${opts.port}/${p.id}/  (ws: /${p.id}/ws)`);
        }
    });

    // Unified WS server. Rather than attach one WSS per plugin, handle
    // `upgrade` manually so we can route by URL before accepting.
    const wss = new WebSocketServer({ noServer: true });

    httpServer.on("upgrade", (req: IncomingMessage, socket: Duplex, head: Buffer) => {
        const url = new URL(req.url ?? "/", `http://${req.headers.host}`);
        // Accept only /${id}/ws style paths.
        const m = url.pathname.match(/^\/([^/]+)\/ws\/?$/);
        if (!m) {
            socket.destroy();
            return;
        }
        const id     = m[1];
        const plugin = plugins.find((p) => p.id === id);
        if (!plugin || !plugin.onUpgrade) {
            socket.destroy();
            return;
        }
        wss.handleUpgrade(req, socket, head, (ws) => {
            plugin.onUpgrade!(req, ws, ctx);
        });
    });

}
