import { MODEL_ROLES, resolveModel } from "@ludiars/one-shot";

export function resolveClaudeSdkModel(model: string, env: NodeJS.ProcessEnv = process.env): string {
    if (!MODEL_ROLES.claude.some((role) => role === model)) return model;

    const resolved = resolveModel(model, "claude", env);
    // One-shot can fall back to a Claude Code alias. The API requires a model ID.
    if (resolved === model) throw new Error(`model_role_unresolved:${model}`);
    return resolved;
}
