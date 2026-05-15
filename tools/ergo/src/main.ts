/// Entry point for the unified ergo developer tool.
///
/// Reads `PORT` from the environment (default 5170), gathers built-in +
/// external plugins, and starts a single HTTP + WS server.

import { PLUGIN_FACTORIES } from "./core/registry.js";
import { loadExternalFactories } from "./core/external.js";
import { boot } from "./core/server.js";

const PORT = Number(process.env.PORT) || 5170;

const external = await loadExternalFactories();

boot({
    port:      PORT,
    factories: [...PLUGIN_FACTORIES, ...external],
});
