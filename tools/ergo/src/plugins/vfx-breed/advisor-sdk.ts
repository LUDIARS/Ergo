/// DirectionAdvisor backed by the Anthropic SDK (ERGO_VFX_BACKEND=sdk,
/// ANTHROPIC_API_KEY from env only).

import Anthropic from "@anthropic-ai/sdk";
import type { ParticleEffectConfig } from "../particle/schema.js";
import type { DirectionAdvisor } from "./advisor.js";
import { buildDirectionPrompt, parseWeights, type DirectionWeights } from "./direction.js";

export function makeSdkAdvisor(model: string, env: NodeJS.ProcessEnv): DirectionAdvisor {
    const apiKey = env["ANTHROPIC_API_KEY"];
    const client = apiKey ? new Anthropic({ apiKey }) : null;

    return {
        health: () => apiKey
            ? { ok: true, backend: "sdk", model }
            : { ok: false, backend: "sdk", model, reason: "ANTHROPIC_API_KEY unset" },

        async suggest(direction: string, current: ParticleEffectConfig): Promise<DirectionWeights> {
            if (!client) throw new Error("ANTHROPIC_API_KEY is not set — cannot use sdk backend");
            const response = await client.messages.create({
                model,
                max_tokens: 1024,
                messages: [{ role: "user", content: buildDirectionPrompt(direction, current) }],
            });
            const text = response.content
                .filter((b): b is Anthropic.TextBlock => b.type === "text")
                .map((b) => b.text)
                .join("\n");
            return parseWeights(text);
        },
    };
}
