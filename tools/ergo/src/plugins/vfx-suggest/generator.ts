/// VfxGenerator interface + factory.
/// Default backend: claude -p CLI (LUDIARS convention).
/// Set ERGO_VFX_BACKEND=sdk to use the Anthropic SDK (requires ANTHROPIC_API_KEY).

import { makeCliGenerator } from "./generator-cli.js";
import { makeSdkGenerator } from "./generator-sdk.js";
import type { GenRequest, VfxPreset } from "./schema.js";

export type { GenRequest, VfxPreset };

export interface VfxGeneratorHealth {
    ok:      boolean;
    backend: string;
    model:   string;
    reason?: string;
}

export interface VfxGenerator {
    generate(req: GenRequest, count: number): Promise<VfxPreset[]>;
    health(): VfxGeneratorHealth;
}

export function makeGenerator(): VfxGenerator {
    const backend = (process.env["ERGO_VFX_BACKEND"] ?? "cli").toLowerCase();
    const model   =  process.env["ERGO_VFX_MODEL"]   ?? "claude-haiku-4-5";

    if (backend === "cli") return makeCliGenerator(model);
    if (backend === "sdk") return makeSdkGenerator(model);

    throw new Error(`Unknown ERGO_VFX_BACKEND="${backend}" — expected "cli" or "sdk"`);
}
