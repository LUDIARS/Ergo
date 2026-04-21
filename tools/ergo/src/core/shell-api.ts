/// Type-only declarations for the in-shell extension API exposed at
/// `window.ergo.shell` (implemented in `public/extensions.js`).
///
/// Plugins running in the iframe cannot import this directly because they
/// live in a separate document, but they can `postMessage` events of the
/// shape `{ type: "ergo:plugin:event", name, payload }` and the shell
/// re-emits them as `plugin:event`.
///
/// External shell extensions (loaded via additional <script> tags in the
/// shell page) can pull in this file purely for the type signatures.

import type { Plugin } from "./plugin.js";

/** Subset of `Plugin` mirrored to the shell over /api/plugins. */
export interface ShellPluginInfo {
    id:           string;
    title:        string;
    description?: string;
    icon?:        string;
    url:          string;
    ws:           string;
}

/** Shape published per plugin under the /api/health response. */
export interface ShellPluginHealth {
    id:    string;
    title: string;
    ok:    boolean;
    [key: string]: unknown;
}

export interface ShellEvents {
    /** Initial plugin list has loaded; payload mirrors /api/plugins. */
    "shell:ready":          { plugins: ShellPluginInfo[] };
    /** Fired once per plugin during initial load. */
    "plugin:registered":    { id: string; plugin: ShellPluginInfo };
    /** A plugin became the active selection in the sidebar. */
    "plugin:activated":     { id: string; plugin?: ShellPluginInfo };
    /** Previously active plugin, fired before the new activation. */
    "plugin:deactivated":   { id: string; plugin?: ShellPluginInfo };
    /** Health snapshot for one plugin (per /api/health poll, every 2 s). */
    "plugin:health":        { id: string; health: ShellPluginHealth };
    /** Re-emit of `postMessage({type:"ergo:plugin:event"})` from the iframe. */
    "plugin:event":         { id: string | null; name: string; payload: unknown };
}

export type ShellEventName = keyof ShellEvents;

export interface ShellApi {
    on<K extends ShellEventName>(
        event: K,
        handler: (payload: ShellEvents[K]) => void,
    ): () => void;
    off<K extends ShellEventName>(
        event: K,
        handler: (payload: ShellEvents[K]) => void,
    ): void;
    emit<K extends ShellEventName>(event: K, payload: ShellEvents[K]): void;
}

/** Optional Electron preload bridge. Only present in the desktop app. */
export interface ShellElectronApi {
    send(channel: "shell:ready" | "shell:plugin-activated", payload: unknown): void;
    on(channel: string, handler: (payload: unknown) => void): () => void;
}

declare global {
    interface Window {
        ergo?: {
            shell?:    ShellApi;
            electron?: ShellElectronApi;
        };
    }
}

// Re-export the plugin definition so extension authors can refer to the
// canonical contract from a single import path.
export type { Plugin };
