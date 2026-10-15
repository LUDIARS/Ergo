/// DirectionAdvisor backed by the claude -p CLI (same launcher as
/// vfx-suggest/generator-cli.ts). The prompt goes through stdin.

import { spawnOneShot as spawn, resolveModel } from "@ludiars/one-shot";
import type { ParticleEffectConfig } from "../particle/schema.js";
import type { DirectionAdvisor } from "./advisor.js";
import { buildDirectionPrompt, parseWeights, type DirectionWeights } from "./direction.js";

export function makeCliAdvisor(model: string, env: NodeJS.ProcessEnv): DirectionAdvisor {
    model = resolveModel(model, "claude");
    const bin = env["CLAUDE_BIN"] ?? "claude";

    return {
        health: () => ({ ok: true, backend: "cli", model }),

        suggest(direction: string, current: ParticleEffectConfig): Promise<DirectionWeights> {
            return new Promise((resolve, reject) => {
                const child = spawn(
                    bin,
                    ["-p", "--model", model, "--output-format", "json"],
                    { cwd: process.cwd(), stdio: ["pipe", "pipe", "pipe"] },
                );
                let out = "";
                let err = "";
                child.stdout.on("data", (d: Buffer) => { out += d.toString(); });
                child.stderr.on("data", (d: Buffer) => { err += d.toString(); });
                child.on("error", (e) => reject(new Error(`claude spawn failed: ${e.message}`)));
                child.on("close", (code) => {
                    if (code !== 0) return reject(new Error(`claude exited ${code}: ${err.slice(0, 300)}`));
                    try {
                        const envelope = JSON.parse(out) as Record<string, unknown>;
                        const text = typeof envelope["result"] === "string" ? envelope["result"] : out;
                        resolve(parseWeights(text));
                    } catch (e) {
                        reject(new Error(`parse failed: ${String(e)}`));
                    }
                });
                child.stdin.write(buildDirectionPrompt(direction, current));
                child.stdin.end();
            });
        },
    };
}
