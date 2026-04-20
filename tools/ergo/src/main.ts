/// Entry point for the unified ergo developer tool.
///
/// Reads `PORT` from the environment (default 5170), instantiates every
/// registered plugin, and starts a single HTTP + WS server.

import { PLUGIN_FACTORIES } from "./core/registry.js";
import { boot } from "./core/server.js";

const PORT = Number(process.env.PORT) || 5170;

boot({
    port:      PORT,
    factories: PLUGIN_FACTORIES,
});
