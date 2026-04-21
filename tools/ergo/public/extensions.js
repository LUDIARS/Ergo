// Shell extension API.
//
// Loaded BEFORE shell.js so any in-browser extension (or the shell itself)
// can subscribe to events from the moment the page boots.
//
// Usage from an extension:
//
//     window.ergo.shell.on("plugin:activated", ({ id, plugin }) => {
//         console.log("now showing", plugin.title);
//     });
//
// Plugins running inside the iframe can post messages up to the shell:
//
//     window.parent.postMessage(
//         { type: "ergo:plugin:event", name: "selection", payload: {...} },
//         "*"
//     );
//
// The shell re-emits these as `plugin:event` with `{ id, name, payload }`,
// where `id` is filled in by the shell from the currently active plugin.

(function () {
    "use strict";

    const listeners = new Map();   // event name -> Set of handlers
    let activeId    = null;        // currently selected plugin id
    let plugins     = [];          // mirror of /api/plugins

    function on(event, handler) {
        if (!listeners.has(event)) listeners.set(event, new Set());
        listeners.get(event).add(handler);
        return () => off(event, handler);
    }

    function off(event, handler) {
        listeners.get(event)?.delete(handler);
    }

    function emit(event, payload) {
        const set = listeners.get(event);
        if (!set) return;
        for (const h of [...set]) {
            try { h(payload); }
            catch (e) { console.error(`[ergo:shell] handler for ${event} threw`, e); }
        }
    }

    /// Internal: shell.js calls these as state changes.
    const _internal = {
        setPlugins(list) {
            plugins = list;
            emit("shell:ready", { plugins });
            for (const p of plugins) emit("plugin:registered", { id: p.id, plugin: p });
        },
        setActive(id) {
            const prev = activeId;
            if (prev === id) return;
            if (prev) {
                const prevPlug = plugins.find((p) => p.id === prev);
                emit("plugin:deactivated", { id: prev, plugin: prevPlug });
            }
            activeId = id;
            const plug = plugins.find((p) => p.id === id);
            emit("plugin:activated", { id, plugin: plug });
        },
        setHealth(snapshot) {
            // snapshot is the /api/health response
            for (const p of snapshot.plugins ?? []) {
                emit("plugin:health", { id: p.id, health: p });
            }
        },
        getActiveId() { return activeId; },
        getPlugins()  { return plugins; },
    };

    // Bridge iframe -> shell: re-emit messages of the agreed shape.
    window.addEventListener("message", (ev) => {
        const data = ev.data;
        if (!data || typeof data !== "object") return;
        if (data.type !== "ergo:plugin:event") return;
        emit("plugin:event", {
            id:      activeId,
            name:    String(data.name ?? ""),
            payload: data.payload,
        });
    });

    // Public surface.
    window.ergo = window.ergo || {};
    window.ergo.shell = { on, off, emit, _internal };

    // If a preload script exposed window.ergo.electron, wire defaults.
    // The preload bridges shell events to the main process so that, e.g.,
    // the window title can reflect the active plugin.
    if (window.ergo.electron && typeof window.ergo.electron.send === "function") {
        on("plugin:activated", ({ id, plugin }) => {
            window.ergo.electron.send("shell:plugin-activated", {
                id, title: plugin?.title ?? id,
            });
        });
        on("shell:ready", ({ plugins }) => {
            window.ergo.electron.send("shell:ready", { count: plugins.length });
        });
    }
})();
