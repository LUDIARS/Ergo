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

import makeParticlePlugin from "../plugins/particle/index.js";
import makeVariablePlugin from "../plugins/variable/index.js";

export const PLUGIN_FACTORIES: PluginFactory[] = [
    makeParticlePlugin,
    makeVariablePlugin,
];
