#include "vfx_prompt.h"

#include "../common/json_pretty.h"
#include "vfx_catalog.h"

namespace ergo::studio::plugins::vfx_suggest {
namespace {

std::string game_context(const std::string& game) {
    if (game == "ks") {
        return "Game: PrivateGame \xE2\x80\x94 top-down C++ bullet-hell survivor. "
               "Skill VFX are resolved by VfxCatalog via keys like Effect/Shockwave, Effect/Lightning. "
               "Style: punchy, short-lived bursts (lifetimeMin < 0.5s typical). Additive blend for most combat hits. "
               "Favor high emission rates and fast speeds. Gravity near 0 or very low for hit sparks.";
    }
    if (game == "ac") {
        return "Game: AdventureCube \xE2\x80\x94 beat-driven C++ action game. "
               "Effects fire on skill events: auto_timed_damage, reactive_decoy, enhancement_bump, auto_timed_projectile. "
               "Style: rhythmic, highly readable pops that sync with music beats. "
               "Moderate lifetime (0.3\xE2\x80\x93" "0.8s). Additive for damage/reactive; alpha for passive/enhancement auras.";
    }
    if (game == "ul") {
        return "Game: UniLand \xE2\x80\x94 explorative world game. "
               "Effects are ambient/environmental and longer-lived. "
               "Style: gentle, atmospheric. Longer lifetimes (1.0\xE2\x80\x93" "3.0s), softer alpha blends, rising particles for magic. "
               "Gravity often negative (particles float up) for nature/magic themes.";
    }
    return "Generic 2D game particle effects. Match the described scene with appropriate style.";
}

double num_at(const JsonValue& cfg, const char* a, const char* b) {
    const JsonValue* x = field(cfg, a);
    const JsonValue* y = x ? field(*x, b) : nullptr;
    return y ? y->as_number() : 0;
}

std::string str_at(const JsonValue& cfg, const char* a, const char* b) {
    const JsonValue* x = field(cfg, a);
    const JsonValue* y = x ? field(*x, b) : nullptr;
    return y ? y->as_string() : std::string();
}

std::string few_shot_block(const std::string& game) {
    std::string out;
    for (const auto& e : catalog_examples(game, 2)) {
        if (!out.empty()) out += "\n";
        out += "Example preset:\n  name: \"" + e.name + "\"\n  rationale: \"" + e.rationale +
               "\"\n  config snippet: emission.rate=" + number_text(num_at(e.config, "emission", "rate")) +
               ", initial.lifetimeMin=" + number_text(num_at(e.config, "initial", "lifetimeMin")) + ", render.blend=\"" +
               str_at(e.config, "render", "blend") + "\"";
    }
    return out;
}

} // namespace

std::string build_system_prompt(const std::string& game) {
    const char* lines[] = {
        "You are a VFX preset generator for the Ergo particle system (schema v1).",
        "Output ONLY a JSON object in the exact shape: {\"presets\":[{\"name\":\"...\",\"rationale\":\"...\",\"config\":{...}},...]}\n",
        "ParticleEffectConfig nested structure (ALL fields required in config):",
        "  emission: { rate: number (1\xE2\x80\x93" "400), maxAlive: number (1\xE2\x80\x93" "2000) }",
        "  initial: { positionRadius, velocityAngleDeg (0\xE2\x80\x93" "360), velocityAngleSpreadDeg (0\xE2\x80\x93" "360),",
        "             speedMin, speedMax (0\xE2\x80\x93" "600), lifetimeMin, lifetimeMax (0.05\xE2\x80\x93" "6.0),",
        "             size (0\xE2\x80\x93" "64), color: [r,g,b,a] normalized 0..1 }",
        "  overLife: { sizeStart, sizeEnd (0\xE2\x80\x93" "8), colorStart:[r,g,b,a], colorEnd:[r,g,b,a],",
        "              velocityDamping (0\xE2\x80\x93" "1) }",
        "  forces: { gravity: [x, y] (-600..600 each). Positive y = down. Negative y = up. }",
        "  render: { blend: \"additive\" | \"alpha\", shape: \"circle\" | \"square\" }",
        "  name: string (effect display name)\n",
        "Rules:",
        "- Fire/lightning/explosion/hit: additive + warm/bright colors fading to alpha 0.",
        "- Heal/buff/magic: additive or alpha + cool/green colors, negative gravity (rising).",
        "- Dust/smoke: alpha + muted colors, slow, size grows over life.",
        "- Do NOT output positionShape, svgSource, svgScale (server fills those).",
        "- Make each preset visually distinct from the others.\n",
    };
    std::string out;
    for (const char* l : lines) out += std::string(l) + "\n";
    out += game_context(game) + "\n\n";
    out += few_shot_block(game);
    return out;
}

std::string build_user_content(const std::string& game, const std::string& scene, int count) {
    return "Game: " + game + "\nScene: \"" + scene + "\"\n\nReturn exactly " + std::to_string(count) +
           " distinct presets as JSON {\"presets\":[...]}.";
}

} // namespace ergo::studio::plugins::vfx_suggest
