/// DirectionAdvisor interface + factory.
///
/// Uses the same backend selection as vfx-suggest's generator
/// (ERGO_VFX_BACKEND = cli | sdk, ERGO_VFX_MODEL). Unlike vfx-suggest the
/// factory never throws: a missing / misconfigured LLM only disables the
/// direction hint, breeding itself keeps working.

import type { ParticleEffectConfig } from "../particle/schema.js";
import type { DirectionWeights } from "./direction.js";
import { makeCliAdvisor } from "./advisor-cli.js";
import { makeSdkAdvisor } from "./advisor-sdk.js";

export interface AdvisorHealth {
    ok:      boolean;
    backend: string;
    model:   string;
    reason?: string;
}

export interface DirectionAdvisor {
    health(): AdvisorHealth;
    suggest(direction: string, current: ParticleEffectConfig): Promise<DirectionWeights>;
}

export function unavailableAdvisor(backend: string, reason: string): DirectionAdvisor {
    return {
        health: () => ({ ok: false, backend, model: "", reason }),
        suggest: () => Promise.reject(new Error(reason)),
    };
}

export function makeAdvisor(env: NodeJS.ProcessEnv = process.env): DirectionAdvisor {
    const backend = (env["ERGO_VFX_BACKEND"] ?? "cli").toLowerCase();
    const model   =  env["ERGO_VFX_MODEL"]   ?? (backend === "cli" ? "haiku" : "claude-haiku-4-5");
    try {
        if (backend === "cli") return makeCliAdvisor(model, env);
        if (backend === "sdk") return makeSdkAdvisor(model, env);
    } catch (e) {
        return unavailableAdvisor(backend, String(e));
    }
    return unavailableAdvisor(backend, `unknown ERGO_VFX_BACKEND="${backend}"`);
}
