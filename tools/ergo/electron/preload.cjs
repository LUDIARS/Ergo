// Electron preload — exposes a tiny IPC bridge to the renderer.
//
// The renderer (shell + extensions.js) calls `window.ergo.electron.send(...)`
// to notify main about shell-level events; main reacts by updating window
// title, menus, etc. Channels are namespaced "shell:*".
//
// `on(channel, handler)` lets the renderer subscribe to push messages
// from main (currently unused but kept symmetric for future extensions).

const { contextBridge, ipcRenderer } = require("electron");

const ALLOWED_OUTBOUND = new Set([
    "shell:ready",
    "shell:plugin-activated",
]);

const ALLOWED_INBOUND = new Set([
    // Reserved for future main-to-renderer channels (e.g., menu-driven
    // commands that should activate a specific plugin).
]);

contextBridge.exposeInMainWorld("ergo", {
    electron: {
        send(channel, payload) {
            if (!ALLOWED_OUTBOUND.has(channel)) {
                console.warn(`[ergo:preload] dropped outbound channel ${channel}`);
                return;
            }
            ipcRenderer.send(channel, payload);
        },
        on(channel, handler) {
            if (!ALLOWED_INBOUND.has(channel)) {
                console.warn(`[ergo:preload] dropped inbound channel ${channel}`);
                return () => {};
            }
            const wrapped = (_event, payload) => handler(payload);
            ipcRenderer.on(channel, wrapped);
            return () => ipcRenderer.off(channel, wrapped);
        },
    },
});
