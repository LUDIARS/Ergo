/// VfxGenerator backed by the Anthropic SDK with tool_use structured output.
/// Enable with ERGO_VFX_BACKEND=sdk. Requires ANTHROPIC_API_KEY env var.
/// Secrets must NOT be hardcoded — env only (feedback_config_and_secrets).

import Anthropic from "@anthropic-ai/sdk";
import type { VfxGenerator, VfxGeneratorHealth } from "./generator.js";
import type { GenRequest, VfxPreset } from "./schema.js";
import { OUTPUT_SCHEMA } from "./schema.js";
import { buildSystemPrompt, buildUserContent } from "./prompt.js";
import { sanitizePresets } from "./sanitize.js";

export function makeSdkGenerator(model: string): VfxGenerator {
    const apiKey = process.env["ANTHROPIC_API_KEY"];
    const client = apiKey ? new Anthropic({ apiKey }) : null;

    return {
        health(): VfxGeneratorHealth {
            if (!apiKey) {
                return { ok: false, backend: "sdk", model, reason: "ANTHROPIC_API_KEY unset" };
            }
            return { ok: true, backend: "sdk", model };
        },

        async generate(req: GenRequest, count: number): Promise<VfxPreset[]> {
            if (!client) {
                throw new Error("ANTHROPIC_API_KEY is not set — cannot use sdk backend");
            }

            const response = await client.messages.create({
                model,
                max_tokens: 4096,
                system: buildSystemPrompt(req.game),
                messages: [{ role: "user", content: buildUserContent(req, count) }],
                tools: [{
                    name:         "output_presets",
                    description:  "Output the generated VFX presets as structured data",
                    input_schema: OUTPUT_SCHEMA as Anthropic.Tool.InputSchema,
                }],
                tool_choice: { type: "tool", name: "output_presets" },
            });

            const toolUse = response.content.find((b): b is Anthropic.ToolUseBlock => b.type === "tool_use");
            if (!toolUse || toolUse.type !== "tool_use") {
                throw new Error(`No tool_use block in response (stop_reason: ${response.stop_reason})`);
            }

            const data = toolUse.input as { presets?: unknown[] };
            return sanitizePresets(data.presets ?? []);
        },
    };
}
