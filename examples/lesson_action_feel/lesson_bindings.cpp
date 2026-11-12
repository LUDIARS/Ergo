#include "lesson_bindings.h"

#include "effect_observations.h"

#include <cstdint>

namespace lesson_action_feel {

using ergo::bind::Value;
using ergo::bind::VarKind;
using ergo::bind::VarMeta;

namespace {

VarMeta writable(const char* category) {
    VarMeta m;
    m.category = category;
    return m;
}

VarMeta observed(const char* category, const char* unit = "") {
    VarMeta m;
    m.category  = category;
    m.read_only = true;
    m.unit      = unit;
    return m;
}

/// No min / max hint on purpose: ergo_bind clamps a write to the hint silently, and this
/// sample refuses an out-of-range write loudly instead (take_rejections()).
VarMeta checked(const char* category, const char* unit = "") {
    VarMeta m = writable(category);
    m.unit    = unit;
    return m;
}

} // namespace

LessonBindings::LessonBindings(FeelToggles& toggles, CombatSettings& settings, const GameSession& session)
    : toggles_(toggles), settings_(settings), session_(session) {}

void LessonBindings::bind_all(ergo::bind::Engine& engine) {
    const ActionGame& game = session_.game();
    for (const EffectInfo& info : effect_table()) {
        const Effect e = info.id;
        engine.bind("feel." + std::string(info.name), &toggles_[e], writable("feel"));
        const bool counted = e == Effect::HitSpark || e == Effect::DamageNumbers || e == Effect::Telegraph ||
                             e == Effect::DefeatMark;
        if (counted) {
            engine.bind_accessor(
                std::string(info.observation), VarKind::Int64,
                [this, e] { return Value::of_int64(static_cast<std::int64_t>(effect_observation(e, session_))); }, {},
                observed("feel"));
        } else {
            engine.bind_accessor(
                std::string(info.observation), VarKind::Double,
                [this, e] { return Value::of_double(effect_observation(e, session_)); }, {},
                observed("feel", e == Effect::HitStop ? "s" : ""));
        }
    }
    engine.bind_accessor(
        "feel.all", VarKind::Bool, [this] { return Value::of_bool(toggles_.all_on()); },
        [this](const Value& v) { toggles_.set_all(v.b); }, writable("feel"));
    engine.bind_accessor(
        "feel.super_armor", VarKind::Bool,
        [this] { return Value::of_bool(settings_.enemy.super_armor != SuperArmor::Off); },
        [this](const Value& v) {
            if (!v.b) settings_.enemy.super_armor = SuperArmor::Off;
            else if (settings_.enemy.super_armor == SuperArmor::Off) settings_.enemy.super_armor = SuperArmor::Attack;
        },
        writable("feel"));
    engine.bind_accessor(
        "feel.motion_vector_player", VarKind::Vec3,
        [this] {
            const Vec2 m = session_.fx().motion_of(session_.game().player().id);
            return Value::of_vec3(m.x, m.y, 0.0f);
        },
        {}, observed("feel", "units/frame"));
    engine.bind_accessor(
        "feel.motion_vector_last_hit", VarKind::Vec3,
        [this] {
            const Vec2 m = session_.fx().motion_of(session_.fx().last_hit_target());
            return Value::of_vec3(m.x, m.y, 0.0f);
        },
        {}, observed("feel", "units/frame"));

    bind_side(engine, "player", settings_.player, game.combat().player);
    bind_side(engine, "enemy", settings_.enemy, game.combat().enemy);

    engine.bind_accessor(
        "game.wave", VarKind::Int32, [this] { return Value::of_int32(session_.game().wave()); }, {}, observed("game"));
    engine.bind_accessor(
        "game.player_hp", VarKind::Int32, [this] { return Value::of_int32(session_.game().player().health.hp()); }, {},
        observed("game"));
    engine.bind_accessor(
        "game.boss_hp", VarKind::Int32,
        [this] {
            const Combatant* b = session_.game().boss();
            return Value::of_int32(b ? b->health.hp() : 0);
        },
        {}, observed("game"));
    engine.bind_accessor(
        "game.defeated", VarKind::Int64, [this] { return Value::of_int64(session_.game().defeated()); }, {},
        observed("game"));
    engine.bind_accessor(
        "game.player_hits", VarKind::Int64, [this] { return Value::of_int64(session_.game().player_hits()); }, {},
        observed("game"));
    engine.bind_accessor(
        "game.score", VarKind::Int64, [this] { return Value::of_int64(session_.game().score()); }, {},
        observed("game"));
}

void LessonBindings::bind_side(ergo::bind::Engine& engine, const std::string& side, SideSettings& s,
                               const SideCounters& n) {
    const std::string p = "combat." + side + ".";
    engine.bind(p + "flinch", &s.flinch, writable("combat"));
    engine.bind(p + "motion_cancel", &s.motion_cancel, writable("combat"));
    const std::string armor_name = p + "super_armor";
    engine.bind_accessor(
        armor_name, VarKind::Int32, [&s] { return Value::of_int32(static_cast<std::int32_t>(s.super_armor)); },
        [this, &s, armor_name](const Value& v) {
            if (v.i < 0 || v.i > 2) {
                rejected_.push_back(armor_name + ": " + std::to_string(v.i) + " refused (0 off, 1 attack, 2 always)");
                return;
            }
            s.super_armor = static_cast<SuperArmor>(v.i);
        },
        checked("combat"));
    const std::string seconds_name = p + "flinch_seconds";
    engine.bind_accessor(
        seconds_name, VarKind::Double, [&s] { return Value::of_double(s.flinch_seconds); },
        [this, &s, seconds_name](const Value& v) {
            if (!(v.d >= 0.0 && v.d <= 5.0)) {
                rejected_.push_back(seconds_name + ": " + std::to_string(v.d) + " refused (0..5)");
                return;
            }
            s.flinch_seconds = static_cast<float>(v.d);
        },
        checked("combat", "s"));
    engine.bind_accessor(
        p + "flinches", VarKind::Int64, [&n] { return Value::of_int64(n.flinches); }, {}, observed("combat"));
    engine.bind_accessor(
        p + "interrupted", VarKind::Int64, [&n] { return Value::of_int64(n.interrupted); }, {}, observed("combat"));
    engine.bind_accessor(
        p + "armor_absorbed", VarKind::Int64, [&n] { return Value::of_int64(n.armor_absorbed); }, {},
        observed("combat"));
    engine.bind_accessor(
        p + "cancels", VarKind::Int64, [&n] { return Value::of_int64(n.cancels); }, {}, observed("combat"));
}

std::vector<std::string> LessonBindings::take_rejections() {
    std::vector<std::string> out;
    out.swap(rejected_);
    return out;
}

} // namespace lesson_action_feel
