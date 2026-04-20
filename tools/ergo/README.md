# ergo (unified developer tool)

Single-port Node server that hosts Ergo's browser-facing developer tools
as in-process plugins. Replaces the separate `particle-editor` and
`variable-editor` packages.

## Ports and URLs

Default port: **5170** (override with `PORT=...`).

| URL                                | Role                                                   |
|------------------------------------|--------------------------------------------------------|
| `http://localhost:5170/`           | Plugin picker shell                                    |
| `http://localhost:5170/particle/`  | Particle editor UI                                     |
| `ws://localhost:5170/particle/ws`  | Particle WS hub (engine clients via `ergo_particle`)   |
| `http://localhost:5170/variable/`  | Variable editor UI                                     |
| `ws://localhost:5170/variable/ws`  | Variable WS hub (engine clients via `ergo_bind`)       |
| `http://localhost:5170/api/health` | Aggregated health of every plugin                      |

## Commands

```bash
npm install
npm run dev       # watch + reload
npm run start     # run once
npm run build     # tsc -> dist/
```

## Adding a plugin

1. Create `src/plugins/<id>/index.ts` exporting a default factory that
   returns a `Plugin` (see `src/core/plugin.ts`).
2. Put any browser assets under `src/plugins/<id>/ui/`.
3. Add the factory to `src/core/registry.ts`.

Each plugin owns its own state and WebSocket handler, but shares the
HTTP/WS bootstrap and the top-level shell UI.

## Migrated from

- `tools/particle-editor/` (port 5173, `/ws`) — now `/particle/ws` at 5170
- `tools/variable-editor/` (port 5174, `/ws`) — now `/variable/ws` at 5170

## Roadmap

- **Phase 2**: migrate `ergo_inspector` to plug in here. The C++
  module's built-in POSIX HTTP/WS server disappears; `ergo_inspector`
  becomes an outbound client like `ergo_bind`, and the existing
  `tools/inspector_web/` page moves under `src/plugins/inspector/ui/`.
