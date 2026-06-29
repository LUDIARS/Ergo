/// Static list of built-in plugins — the *generic* editors that ship
/// with the tool and apply to any host project.
///
/// Game-specific plugins (level / stage / spawn editors etc.) are NOT
/// listed here. They live in the host repo and are loaded at runtime via
/// `ERGO_PLUGIN_DIR` — see core/external.ts. This keeps the shared tool
/// free of per-game features:
///   KuzuSurvivors -> tools/kzs-web/plugins/{spawn,skill}
///   AdventureCube -> tools/ac-web/plugins/{placer,terrain,acstage}

import type { PluginFactory } from "./plugin.js";

import makeParticlePlugin       from "../plugins/particle/index.js";
import makeVariablePlugin       from "../plugins/variable/index.js";
import makeRivePlugin           from "../plugins/rive/index.js";
import makeProfilePlugin        from "../plugins/profile/index.js";
import makePictorPerfPlugin     from "../plugins/pictor_perf/index.js";
import makeRenderPipelinePlugin from "../plugins/render_pipeline/index.js";
import makeVisusPlugin          from "../plugins/visus/index.js";
import makeUiLayoutPlugin       from "../plugins/ui_layout/index.js";
import makeVfxSuggestPlugin     from "../plugins/vfx-suggest/index.js";
import makeSceneEditorPlugin    from "../plugins/scene_editor/index.js";

export const PLUGIN_FACTORIES: PluginFactory[] = [
    makeParticlePlugin,
    makeVariablePlugin,
    makeRivePlugin,
    makeProfilePlugin,
    makePictorPerfPlugin,
    makeRenderPipelinePlugin,
    makeVisusPlugin,
    makeUiLayoutPlugin,
    makeVfxSuggestPlugin,
    makeSceneEditorPlugin,
];
