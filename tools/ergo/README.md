# ergo (unified developer tool)

Single-port Node server that hosts Ergo's browser-facing developer tools
as in-process plugins. Replaces the separate `particle-editor` and
`variable-editor` packages.

**Default launch mode is a standalone Electron desktop app.** A pure
headless-server mode is kept as a fallback for CI / remote use.

## Ports and URLs

Default port: **5170** (override with `PORT=...`).

| URL                                | Role                                                   |
|------------------------------------|--------------------------------------------------------|
| `http://localhost:5170/`           | Plugin picker shell (also loaded by the Electron app)  |
| `http://localhost:5170/particle/`  | Particle editor UI                                     |
| `ws://localhost:5170/particle/ws`  | Particle WS hub (engine clients via `ergo_particle`)   |
| `http://localhost:5170/variable/`  | Variable editor UI                                     |
| `ws://localhost:5170/variable/ws`  | Variable WS hub (engine clients via `ergo_bind`)       |
| `http://localhost:5170/api/health` | Aggregated health of every plugin                      |

Engine clients always speak to these URLs regardless of whether the
server is running inside the Electron app or headless.

## Commands

```bash
npm install

# Primary: standalone Electron desktop app (builds + launches a window).
npm start            # alias: npm run app

# Headless server only (no window) — open your own browser or connect
# engine clients. Useful for CI, docker, remote workstations, or when
# you already have a preferred browser stack.
npm run serve        # one-shot
npm run dev          # watch + reload

# Just build the TS sources to dist/ (used by `npm start`).
npm run build
```

## Why Electron (not Tauri)?

The backend is pure Node (Hono + `ws`). Electron hosts that process
natively, so the app is a thin shell around the existing server. Tauri
would force either a Rust rewrite of the backend or bundling Node as a
sidecar — both significantly more work for the same user-facing result.
If the backend ever moves to Rust/WebGPU, a Tauri swap is on the table.

## Adding a plugin

1. Create `src/plugins/<id>/index.ts` exporting a default factory that
   returns a `Plugin` (see `src/core/plugin.ts`).
2. Put any browser assets under `src/plugins/<id>/ui/`.
3. Add the factory to `src/core/registry.ts`.

Each plugin owns its own state and WebSocket handler, but shares the
HTTP/WS bootstrap and the top-level shell UI.

## Extending the shell window

The shell UI exposes an event bus at `window.ergo.shell` for adding
behavior without forking `shell.js`. Subscribe to e.g. `plugin:activated`
to react when a plugin is selected from the sidebar:

```js
// drop a <script> tag in public/index.html (after extensions.js):
window.ergo.shell.on("plugin:activated", ({ id, plugin }) => {
    console.log("now showing:", plugin.title);
});
```

Plugins running in the iframe can also push events up:

```js
// inside a plugin UI
window.parent.postMessage(
    { type: "ergo:plugin:event", name: "selection", payload: {...} },
    "*"
);
```

The Electron app additionally exposes `window.ergo.electron.send(channel,
payload)` (declared in `electron/preload.cjs`) for renderer→main IPC. The
default reaction is to retitle the window to the active plugin. Full
event list, type signatures, and IPC channel allowlist live in
`spec/tool/ergo.md` § "シェル拡張 API".

## Migrated from

- `tools/particle-editor/` (port 5173, `/ws`) — now `/particle/ws` at 5170
- `tools/variable-editor/` (port 5174, `/ws`) — now `/variable/ws` at 5170

## Roadmap

- ~~**Phase 2**: migrate `ergo_inspector` to plug in here.~~ Cancelled
  2026-04-21 — `ergo_inspector` was a strict subset of `ergo_bind` and was
  removed from the engine entirely. New live-tuning features land in
  `ergo_bind` + the `variable` plugin.
- **Packaging**: ship signed `ergo.exe` / `ergo.app` via electron-builder
  when there's a clear distribution need (currently run from source).
