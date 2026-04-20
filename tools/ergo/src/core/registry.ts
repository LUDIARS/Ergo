/// Static list of built-in plugins. Add new plugins by importing their
/// factory here and appending it to `PLUGIN_FACTORIES`.
///
/// Dynamic / user-installed plugins are out of scope for now — every
/// plugin ships with the tool.

import type { PluginFactory } from "./plugin.js";

import makeParticlePlugin from "../plugins/particle/index.js";
import makeVariablePlugin from "../plugins/variable/index.js";

export const PLUGIN_FACTORIES: PluginFactory[] = [
    makeParticlePlugin,
    makeVariablePlugin,
    // Phase 2: makeInspectorPlugin,
];
