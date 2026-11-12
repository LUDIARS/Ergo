#include "lesson_bindings.h"

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

LessonBindings::LessonBindings(FeelToggles& toggles, CombatSettings& settings, const ActionGame& game)
    : toggles_(toggles), settings_(settings), game_(game) {}

void LessonBindings::bind_all(ergo::bind::Engine& engine) {
    engine.bind("feel.hit_stop", &toggles_.hit_stop, writable("feel"));
    engine.bind("feel.knockback", &toggles_.knockback, writable("feel"));
    engine.bind("feel.camera_shake", &toggles_.camera_shake, writable("feel"));
    engine.bind_accessor(
        "feel.super_armor", VarKind::Bool,
        [this] { return Value::of_bool(settings_.enemy.super_armor != SuperArmor::Off); },
        [this](const Value& v) {
            if (!v.b) settings_.enemy.super_armor = SuperArmor::Off;
            else if (settings_.enemy.super_armor == SuperArmor::Off) settings_.enemy.super_armor = SuperArmor::Attack;
        },
        writable("feel"));
    engine.bind_accessor(
        "feel.hit_stop_last_seconds", VarKind::Double,
        [this] { return Value::of_double(game_.observed().hit_stop_last_seconds); }, {}, observed("feel", "s"));
    engine.bind_accessor(
        "feel.knockback_last_impulse", VarKind::Double,
        [this] { return Value::of_double(game_.observed().knockback_last_impulse); }, {}, observed("feel"));

    bind_side(engine, "player", settings_.player, game_.combat().player);
    bind_side(engine, "enemy", settings_.enemy, game_.combat().enemy);

    engine.bind_accessor(
        "game.wave", VarKind::Int32, [this] { return Value::of_int32(game_.wave()); }, {}, observed("game"));
    engine.bind_accessor(
        "game.player_hp", VarKind::Int32, [this] { return Value::of_int32(game_.player().health.hp()); }, {},
        observed("game"));
    engine.bind_accessor(
        "game.defeated", VarKind::Int64, [this] { return Value::of_int64(game_.defeated()); }, {}, observed("game"));
    engine.bind_accessor(
        "game.player_hits", VarKind::Int64, [this] { return Value::of_int64(game_.player_hits()); }, {},
        observed("game"));
    engine.bind_accessor(
        "game.score", VarKind::Int64, [this] { return Value::of_int64(game_.score()); }, {}, observed("game"));
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
