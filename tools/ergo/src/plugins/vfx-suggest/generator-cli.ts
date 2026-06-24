/// VfxGenerator backed by the claude -p CLI (LUDIARS default).
/// Long prompts are written to stdin (feedback_claude_cli_long_prompt).
/// Requires claude CLI on PATH; on Windows use git-bash (feedback_claude_cli_windows_bash).

import { spawn } from "node:child_process";
import type { VfxGenerator, VfxGeneratorHealth } from "./generator.js";
import type { GenRequest, VfxPreset } from "./schema.js";
import { buildSystemPrompt, buildUserContent } from "./prompt.js";
import { sanitizePresets, extractJson } from "./sanitize.js";

export function makeCliGenerator(model: string): VfxGenerator {
    const bin = process.env["CLAUDE_BIN"] ?? "claude";

    return {
        health(): VfxGeneratorHealth {
            return { ok: true, backend: "cli", model };
        },

        generate(req: GenRequest, count: number): Promise<VfxPreset[]> {
            return new Promise((resolve, reject) => {
                const systemPart = buildSystemPrompt(req.game);
                const userPart   = buildUserContent(req, count);
                // Combine system + user in a single stdin message for robustness.
                const fullPrompt = `${systemPart}\n\n---\n\n${userPart}`;

                const child = spawn(
                    bin,
                    ["-p", "--model", model, "--output-format", "json"],
                    { stdio: ["pipe", "pipe", "pipe"] },
                );

                let out = "";
                let err = "";
                child.stdout.on("data", (d: Buffer) => { out += d.toString(); });
                child.stderr.on("data", (d: Buffer) => { err += d.toString(); });

                child.on("error", (e) => reject(new Error(`claude spawn failed: ${e.message}`)));
                child.on("close", (code) => {
                    if (code !== 0) {
                        return reject(new Error(`claude exited ${code}: ${err.slice(0, 300)}`));
                    }
                    try {
                        // --output-format json wraps result in {result:"..."} envelope.
                        const envelope = JSON.parse(out) as Record<string, unknown>;
                        const text     = typeof envelope["result"] === "string"
                            ? envelope["result"]
                            : out;
                        const json     = extractJson(text);
                        const data     = JSON.parse(json) as { presets?: unknown[] };
                        resolve(sanitizePresets(data.presets ?? []));
                    } catch (e) {
                        reject(new Error(`parse failed: ${String(e)}\nraw: ${out.slice(0, 200)}`));
                    }
                });

                child.stdin.write(fullPrompt);
                child.stdin.end();
            });
        },
    };
}
