/// Plugin contract for the unified `ergo` developer tool.
///
/// Each plugin owns a namespaced slice of the server:
///   HTTP: /${id}/*
///   WS:   /${id}/ws
///
/// The shared server (src/core/server.ts) mounts every plugin on startup
/// and hands WebSocket upgrade events to `onUpgrade`. Plugins should keep
/// their state encapsulated in the factory closure so adding / removing a
/// plugin is always local to that plugin's directory.

import type { Hono } from "hono";
import type { IncomingMessage } from "node:http";
import type { WebSocket } from "ws";

export interface PluginContext {
    /** Tagged console logging — the shell UI can display recent log lines. */
    log: (level: "info" | "warn" | "error", msg: string) => void;
}

export interface PluginHealth {
    ok: boolean;
    /** Free-form fields. Common: `clients`, `version`, `state`. */
    [key: string]: unknown;
}

export interface Plugin {
    /** Path segment. Mounted at /${id}/ on HTTP and /${id}/ws on WS. */
    id: string;

    /** Display name for the shell picker. */
    title: string;

    /** Short one-line blurb (shell tooltip). */
    description?: string;

    /** Emoji / icon string used by the shell (optional). */
    icon?: string;

    /** Path (relative to cwd) to serve as the plugin's browser UI.
     *  Requests to /${id}/ redirect to /${id}/index.html which is read
     *  from `${staticRoot}/index.html`. Additional files are served from
     *  the same root. */
    staticRoot?: string;

    /** HTTP sub-app (Hono). Mounted at /${id}. Typically only registers
     *  API routes under /api/*; static UI is served by the shared
     *  server based on `staticRoot`. */
    routes?(ctx: PluginContext): Hono;

    /** Called for every WS upgrade under /${id}/ws. */
    onUpgrade?(req: IncomingMessage, ws: WebSocket, ctx: PluginContext): void;

    /** Health snapshot, shown under /api/health. */
    health?(): PluginHealth;
}

/** Plugin factory — called once per server start. */
export type PluginFactory = () => Plugin;
