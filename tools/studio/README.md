# Ergo Studio (native developer tool)

One C++ desktop app, `ergo_studio.exe`, that hosts Ergo's developer tools
as in-process plugins on a single port. The window is GLFW + Vulkan +
Pictor's UI renderer; engine clients and host programs talk to the same
HTTP / WebSocket routes the former Node / Electron tool (`tools/ergo`,
removed 2026-11-06) served. The full spec, including the plugin matrix
(which C++ module and which engine-side client feeds each plugin), is
[`spec/feature/tool/ergo.md`](../../spec/feature/tool/ergo.md).

## Ports and URLs

Default port: **5170** (override with `PORT=...`).

| URL                                      | Role                                                   |
|------------------------------------------|--------------------------------------------------------|
| `http://localhost:5170/api/plugins`      | Registered plugins (the window's tabs)                 |
| `http://localhost:5170/api/health`       | Aggregated health of every plugin, plus skipped external entries under `rejected` |
| `http://localhost:5170/studio/`          | Ergo Studio — build games with AI alongside LUDIARS services (Cc / Actio / Br / Curare / Revisor). See `spec/feature/tool/studio.md` |
| `http://localhost:5170/particle/`        | Particle editor. WS `/particle/ws`; engine client `ergo_particle_link` (receive-only) |
| `http://localhost:5170/variable/`        | Variable editor. WS `/variable/ws`; engine client `ergo_bind` (`Engine::connect`) |
| `http://localhost:5170/rive/`            | Rive metadata relay                                    |
| `http://localhost:5170/profile/`         | Profile timeline for `ergo_profile` Chrome Trace JSON  |
| `http://localhost:5170/pictor_perf/`     | Pictor perf snapshots (host publishes over WS or `POST /pictor_perf/api/publish`) |
| `http://localhost:5170/render_pipeline/` | Pictor `*.profile.json` graph editor (`ERGO_PICTOR_PROFILE_DIR`) |
| `http://localhost:5170/visus/`           | Visus definition editor (`VISUS_PROJECT_ROOTS`)        |
| `http://localhost:5170/ui_layout/`       | UI layout editor; live bridge to the host's own `/ui_layout/{patch,state}` |
| `http://localhost:5170/lookdev/`         | Look-dev: tune a game's lighting live, save presets, post a still to Discord / Slack |
| `http://localhost:5170/vfx-suggest/`     | Particle presets from game / scene context via an LLM  |
| `http://localhost:5170/vfx-breed/`       | Breed particle effects by picking favourites           |
| `http://localhost:5170/scene_editor/`    | Scene tree and actor inspector. WS `/scene_editor/ws`; engine client `ergo_scene_link` |
| `http://localhost:5170/log/`             | Live `ergo_log` viewer. API `/log/api/apps`, `/log/api/lines`; WS `/log/ws`; engine client `ergo_log_live` |
| `http://localhost:5170/musa_melpomene/`  | Ars-Musa Melpomene: GitHub issues by scene / object / position, PR and run notifications |
| `http://localhost:5170/musa_terpsichore/`| Ars-Musa Terpsichore: configure / start / stop the command server |
| `http://localhost:5170/musa_thaleia/`    | Ars-Musa Thaleia: spec document tree, viewer, categories and search |

Each plugin's REST lives under `/<id>/api/*` (every plugin has
`GET /<id>/api/health`); the UI is the app window, not a browser page.

## Build and run

```bash
# From the repo root (needs the Vulkan SDK for the window)
cmake -S . -B build -DERGO_PACKAGES=pictor -DERGO_PACKAGES_FETCH=ON
cmake --build build --config Release --target ergo_studio
build/studio/ergo_studio.exe              # window
build/studio/ergo_studio.exe --headless   # server only
```

Without Pictor the same target builds a headless-only exe. In the
LUDIARS environment start it through Excubitor (`ergo-studio` in
`excubitor.catalog.yaml`), not by hand.

Environment variables are listed in
[`spec/setup/config-reference.md`](../../spec/setup/config-reference.md).

## Layout

- `main.cpp` / `studio_settings.*` — the exe; the only place that reads the environment
- `builtin_plugins.cpp` — the built-in plugin registry (checked by ctest `test_module_catalog`)
- `plugins/<dir>/` — one built-in plugin per directory; `plugins/common/` holds shared parts
- Libraries: `src/studio/` + `include/ergo/studio/` (net / host / platform / ui / shell)
- External plugin packs: C ABI in `include/ergo/studio/plugin_abi.h`, loaded from `ERGO_PLUGIN_DIR`
- Tests: `tests/studio/` (ctest)
