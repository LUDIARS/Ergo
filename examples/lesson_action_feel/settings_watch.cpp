#include "settings_watch.h"

#include <cstdio>

namespace lesson_action_feel {

namespace {

const char* on_off(bool b) { return b ? "ON" : "OFF"; }

std::string seconds(float s) {
    char      buf[32];
    const int n = std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(s));
    if (n < 0 || n >= static_cast<int>(sizeof(buf))) return "?";
    return buf;
}

void flag(std::vector<std::string>& out, const char* name, bool before, bool now) {
    if (before != now) out.push_back(std::string(name) + ": " + on_off(before) + " -> " + on_off(now));
}

void side(std::vector<std::string>& out, const std::string& prefix, const SideSettings& a, const SideSettings& b) {
    flag(out, (prefix + "flinch").c_str(), a.flinch, b.flinch);
    if (a.flinch_seconds != b.flinch_seconds)
        out.push_back(prefix + "flinch_seconds: " + seconds(a.flinch_seconds) + " -> " + seconds(b.flinch_seconds));
    if (a.super_armor != b.super_armor)
        out.push_back(prefix + "super_armor: " + super_armor_name(a.super_armor) + " -> " +
                      super_armor_name(b.super_armor));
    flag(out, (prefix + "motion_cancel").c_str(), a.motion_cancel, b.motion_cancel);
}

std::string side_text(const char* who, const SideSettings& s) {
    return std::string(who) + " flinch=" + on_off(s.flinch) + " super_armor=" + super_armor_name(s.super_armor) +
           " motion_cancel=" + on_off(s.motion_cancel);
}

} // namespace

std::vector<std::string> SettingsWatch::changes(const FeelToggles& t, const CombatSettings& s) {
    std::vector<std::string> out;
    flag(out, "feel.hit_stop", toggles_.hit_stop, t.hit_stop);
    flag(out, "feel.knockback", toggles_.knockback, t.knockback);
    flag(out, "feel.camera_shake", toggles_.camera_shake, t.camera_shake);
    side(out, "combat.enemy.", settings_.enemy, s.enemy);
    side(out, "combat.player.", settings_.player, s.player);
    toggles_  = t;
    settings_ = s;
    return out;
}

std::string describe_settings(const FeelToggles& t, const CombatSettings& s) {
    return std::string("hit_stop=") + on_off(t.hit_stop) + " knockback=" + on_off(t.knockback) +
           " camera_shake=" + on_off(t.camera_shake) + "; " + side_text("enemy", s.enemy) + "; " +
           side_text("player", s.player);
}

} // namespace lesson_action_feel
