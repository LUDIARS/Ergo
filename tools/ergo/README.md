# ergo (unified developer tool)

Single-port Node server that hosts Ergo's browser-facing developer tools
as in-process plugins. Replaces the separate `particle-editor` and
`variable-editor` packages. The full spec, including the plugin matrix
(which C++ module and which engine-side client feeds each plugin), is
[`spec/feature/tool/ergo.md`](../../spec/feature/tool/ergo.md).

**Default launch mode is a standalone Electron desktop app.** A pure
headless-server mode is kept as a fallback for CI / remote use.

## Ports and URLs

Default port: **5170** (override with `PORT=...`).

| URL                                      | Role                                                   |
|------------------------------------------|--------------------------------------------------------|
| `http://localhost:5170/`                 | Plugin picker shell (also loaded by the Electron app)  |
| `http://localhost:5170/api/plugins`      | Registered plugins (the shell's sidebar)               |
| `http://localhost:5170/api/health`       | Aggregated health of every plugin, plus skipped external entries under `rejected` |
| `http://localhost:5170/particle/`        | Particle editor. WS `/particle/ws`; engine client `ergo_particle_link` (receive-only) |
| `http://localhost:5170/variable/`        | Variable editor. WS `/variable/ws`; engine client `ergo_bind` (`Engine::connect`) |
| `http://localhost:5170/rive/`            | Rive player (`.riv` files are read in the browser)     |
| `http://localhost:5170/profile/`         | Profile timeline for `ergo_profile` Chrome Trace JSON  |
| `http://localhost:5170/pictor_perf/`     | Pictor perf snapshots (host publishes over WS or `POST /pictor_perf/api/publish`) |
| `http://localhost:5170/render_pipeline/` | Pictor `*.profile.json` graph editor (`ERGO_PICTOR_PROFILE_DIR`) |
| `http://localhost:5170/visus/`           | Visus definition editor (`VISUS_PROJECT_ROOTS`)        |
| `http://localhost:5170/ui_layout/`       | UI layout editor; live bridge to the host's own `/ui_layout/{patch,state}` |
| `http://localhost:5170/vfx-suggest/`     | Particle presets from game / scene context via an LLM  |
| `http://localhost:5170/vfx-breed/`       | Breed particle effects by picking favourites           |
| `http://localhost:5170/scene_editor/`    | Scene tree and actor inspector. WS `/scene_editor/ws`; engine client `ergo_scene_link` |
| `http://localhost:5170/log/`             | Live `ergo_log` viewer. API `/log/api/apps`, `/log/api/lines`; WS `/log/ws`; engine client `ergo_log_live` |

Engine clients always speak to these URLs regardless of whether the
server is running inside the Electron app or headless.

## Prerequisites

- Node `>= 22.12.0` (`engines` in `package.json`).
- The `lib/lapilli` submodule (the shared CLI launcher `@ludiars/one-shot`,
  imported by `vfx-suggest` / `vfx-breed`). Without it the whole server fails
  to start: run `git submodule update --init -- lib/lapilli` from the repo root.

New to the repo? [`spec/setup/quickstart.md`](../../spec/setup/quickstart.md)
goes from clone to editing a running program from the browser.

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

Environment variables (`PORT`, `ERGO_HOST`, `ERGO_PLUGIN_DIR`, the plugin
data roots and the LLM settings of `vfx-suggest` / `vfx-breed`) are listed in
[`spec/setup/config-reference.md`](../../spec/setup/config-reference.md).
`ANTHROPIC_API_KEY` is a secret: pass it in the shell environment only.

Binding to 127.0.0.1 does not keep web pages out, so the server answers 403
to cross-site requests: an HTTP request whose `Host` is not localhost, an IP
literal or `ERGO_HOST`, or whose `Origin` is another site, and a WebSocket
handshake with a foreign `Origin` (engines send none). See "接続元の制限" in
[`spec/feature/tool/ergo.md`](../../spec/feature/tool/ergo.md).

## Testing

```bash
npm test             # node:test + tsx over tests/**/*.test.ts
npm run typecheck    # tsc -p tsconfig.test.json (src + tests, no output)
```

Tests live under `tests/core/`, `tests/helpers/` and `tests/plugins/<id>/`
and are hermetic (temp dirs, port 0, no network). CI runs the same steps in
the `tools-ergo` job of `.github/workflows/ci.yml`
([`spec/setup/ci.md`](../../spec/setup/ci.md)).

## Why Electron (not Tauri)?

The backend is pure Node (Hono + `ws`). Electron hosts that process
natively, so the app is a thin shell around the existing server. Tauri
would force either a Rust rewrite of the backend or bundling Node as a
sidecar — both significantly more work for the same user-facing result.
If the backend ever moves to Rust/WebGPU, a Tauri swap is on the table.

## Adding a plugin

1. Create `src/plugins/<id>/index.ts` exporting a default factory that
   returns a `Plugin` (see `src/core/plugin.ts`). The id must match
   `/^[a-z0-9][a-z0-9_-]*$/` and must not be `api` or `shell`.
2. Put any browser assets under `src/plugins/<id>/ui/` and serve them through
   `staticRoot`; add a `GET /api/health` route that returns `{ ok: boolean }`
   (`tests/core/server.test.ts` checks both for every built-in).
3. Add the factory to `src/core/registry.ts`, a row to the plugin matrix in
   `spec/feature/tool/ergo.md` and a URL row above (`ctest -R
   test_module_catalog` checks that all three agree).
4. Put its tests under `tests/plugins/<id>/`.

Each plugin owns its own state and WebSocket handler, but shares the
HTTP/WS bootstrap and the top-level shell UI. Game-specific editors do not
belong here: keep them in the host repo as an external plugin pack.

## External plugin packs

`ERGO_PLUGIN_DIR` (OS path separator, several allowed) names plugin packs in
host repos; every subdirectory with an `index.js` / `index.ts` is loaded on
top of the built-ins. A broken pack entry is logged and skipped, never fatal,
and the first plugin to claim an id wins (built-ins first). See
[`spec/setup/external-plugins.md`](../../spec/setup/external-plugins.md).

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
[`spec/feature/tool/ergo.md`](../../spec/feature/tool/ergo.md) § "シェル拡張 API".

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
