/// Builds system and user prompts for LLM-based VFX generation.

import type { GameId, GenRequest } from "./schema.js";
import { catalogExamples } from "./catalog.js";

const GAME_CONTEXT: Record<GameId, string> = {
    ks: [
        "Game: KuzuSurvivors — top-down C++ bullet-hell survivor.",
        "Skill VFX are resolved by VfxCatalog via keys like Effect/Shockwave, Effect/Lightning.",
        "Style: punchy, short-lived bursts (lifetimeMin < 0.5s typical). Additive blend for most combat hits.",
        "Favor high emission rates and fast speeds. Gravity near 0 or very low for hit sparks.",
    ].join(" "),
    ac: [
        "Game: AdventureCube — beat-driven C++ action game.",
        "Effects fire on skill events: auto_timed_damage, reactive_decoy, enhancement_bump, auto_timed_projectile.",
        "Style: rhythmic, highly readable pops that sync with music beats.",
        "Moderate lifetime (0.3–0.8s). Additive for damage/reactive; alpha for passive/enhancement auras.",
    ].join(" "),
    ul: [
        "Game: UniLand — explorative world game.",
        "Effects are ambient/environmental and longer-lived.",
        "Style: gentle, atmospheric. Longer lifetimes (1.0–3.0s), softer alpha blends, rising particles for magic.",
        "Gravity often negative (particles float up) for nature/magic themes.",
    ].join(" "),
    generic: "Generic 2D game particle effects. Match the described scene with appropriate style.",
};

function fewShotBlock(game: GameId): string {
    const examples = catalogExamples(game, 2);
    return examples.map(e =>
        `Example preset:\n  name: "${e.name}"\n  rationale: "${e.rationale}"\n  config snippet: emission.rate=${e.config.emission.rate}, initial.lifetimeMin=${e.config.initial.lifetimeMin}, render.blend="${e.config.render.blend}"`
    ).join("\n");
}

export function buildSystemPrompt(game: GameId): string {
    return [
        "You are a VFX preset generator for the Ergo particle system (schema v1).",
        "Output ONLY a JSON object in the exact shape: {\"presets\":[{\"name\":\"...\",\"rationale\":\"...\",\"config\":{...}},...]}\n",
        "ParticleEffectConfig nested structure (ALL fields required in config):",
        "  emission: { rate: number (1–400), maxAlive: number (1–2000) }",
        "  initial: { positionRadius, velocityAngleDeg (0–360), velocityAngleSpreadDeg (0–360),",
        "             speedMin, speedMax (0–600), lifetimeMin, lifetimeMax (0.05–6.0),",
        "             size (0–64), color: [r,g,b,a] normalized 0..1 }",
        "  overLife: { sizeStart, sizeEnd (0–8), colorStart:[r,g,b,a], colorEnd:[r,g,b,a],",
        "              velocityDamping (0–1) }",
        "  forces: { gravity: [x, y] (-600..600 each). Positive y = down. Negative y = up. }",
        "  render: { blend: \"additive\" | \"alpha\", shape: \"circle\" | \"square\" }",
        "  name: string (effect display name)\n",
        "Rules:",
        "- Fire/lightning/explosion/hit: additive + warm/bright colors fading to alpha 0.",
        "- Heal/buff/magic: additive or alpha + cool/green colors, negative gravity (rising).",
        "- Dust/smoke: alpha + muted colors, slow, size grows over life.",
        "- Do NOT output positionShape, svgSource, svgScale (server fills those).",
        "- Make each preset visually distinct from the others.\n",
        GAME_CONTEXT[game] + "\n",
        fewShotBlock(game),
    ].join("\n");
}

export function buildUserContent(req: GenRequest, count: number): string {
    return `Game: ${req.game}\nScene: "${req.scene}"\n\nReturn exactly ${count} distinct presets as JSON {"presets":[...]}.`;
}
