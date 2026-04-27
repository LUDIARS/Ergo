/// Static list of built-in plugins. Add new plugins by importing their
/// factory here and appending it to `PLUGIN_FACTORIES`.
///
/// Dynamic / user-installed plugins are out of scope for now — every
/// plugin ships with the tool.

import type { PluginFactory } from "./plugin.js";

import makeParticlePlugin from "../plugins/particle/index.js";
import makeVariablePlugin from "../plugins/variable/index.js";
import makePlacerPlugin   from "../plugins/placer/index.js";
import makeTerrainPlugin  from "../plugins/terrain/index.js";
import makeACStagePlugin  from "../plugins/acstage/index.js";

export const PLUGIN_FACTORIES: PluginFactory[] = [
    makeParticlePlugin,
    makeVariablePlugin,
    makePlacerPlugin,
    makeTerrainPlugin,
    makeACStagePlugin,
    // Phase 2: makeInspectorPlugin,
];
