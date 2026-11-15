#include "arena_renderer.h"
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>

namespace lesson_action_feel::graphical {
namespace {
using namespace Gdiplus;
constexpr float pi = 3.14159265f;
void text(Graphics& g, const std::wstring& value, float x, float y, float size, Color color) {
    Font font(L"Segoe UI", size, FontStyleRegular, UnitPixel);
    SolidBrush ink(color);
    g.DrawString(value.c_str(), static_cast<INT>(value.size()), &font, PointF(x, y), &ink);
}
void bar(Graphics& g, float x, float y, float w, float h, float fraction, Color color) {
    SolidBrush back(Color(255, 37, 47, 67)), fill(color);
    g.FillRectangle(&back, x, y, w, h);
    g.FillRectangle(&fill, x, y, w * std::clamp(fraction, 0.0f, 1.0f), h);
}
void fighter(Graphics& g, const FighterView& f, const Viewport& v) {
    if (!f.visible) return;
    const float x = v.left + f.pos.x * v.scale, y = v.top + f.pos.y * v.scale;
    const float r = f.radius * v.scale;
    const bool player = f.side == Side::Player;
    const Color color = player ? Color(255, 89, 231, 215) :
                        f.boss ? Color(255, 197, 131, 255) : Color(255, 255, 119, 136);
    const float angle = std::atan2(f.facing.y, f.facing.x) * 180.0f / pi;
    const float arc = std::acos(std::clamp(f.arc_cos, -1.0f, 1.0f)) * 180.0f / pi;
    const float reach = f.reach * v.scale;
    if (f.telegraph || (f.action == Action::Attack && f.phase == AttackPhase::Active)) {
        const bool active = f.phase == AttackPhase::Active;
        SolidBrush fill(Color(active ? 140 : 35, color.GetR(), color.GetG(), color.GetB()));
        Pen edge(color, active ? 3.0f : 1.0f);
        g.FillPie(&fill, x - reach, y - reach, 2 * reach, 2 * reach, angle - arc, 2 * arc);
        g.DrawArc(&edge, x - reach, y - reach, 2 * reach, 2 * reach, angle - arc, 2 * arc);
    }
    if (!f.streak.is_zero()) {
        Pen trail(Color(90, color.GetR(), color.GetG(), color.GetB()), r);
        g.DrawLine(&trail, x, y, x + f.streak.x * v.scale, y + f.streak.y * v.scale);
    }
    SolidBrush shadow(Color(100, 0, 0, 0));
    g.FillEllipse(&shadow, x - r, y + r * .4f, r * 2, r);
    SolidBrush body(f.spark ? Color::White : f.action == Action::Dead ? Color(255, 65, 73, 89) : color);
    g.FillEllipse(&body, x - r, y - r, r * 2, r * 2);
    Pen rim(f.armored_now ? Color(255, 255, 215, 110) : Color(255, 220, 242, 248), f.armored_now ? 3.0f : 1.0f);
    g.DrawEllipse(&rim, x - r, y - r, r * 2, r * 2);
    Pen blade(Color(255, 240, 247, 255), player ? 4.0f : 2.0f);
    g.DrawLine(&blade, x + f.facing.x * r, y + f.facing.y * r,
               x + f.facing.x * r * 1.8f, y + f.facing.y * r * 1.8f);
    if (f.action != Action::Dead) {
        bar(g, x - r - 5, y - r - 12, r * 2 + 10, 4,
            f.max_hp > 0 ? static_cast<float>(f.hp) / f.max_hp : 0, color);
    }
    if (f.spark) {
        Pen spark(Color::White, 2);
        for (int i = 0; i < 8; ++i) {
            const float a = i * pi / 4;
            g.DrawLine(&spark, x + std::cos(a) * r * 1.3f, y + std::sin(a) * r * 1.3f,
                       x + std::cos(a) * r * 2, y + std::sin(a) * r * 2);
        }
    }
}
} // namespace

Viewport viewport(int width, int height, const GameView& view) {
    const float scale = std::max(1.0f, std::min((width - 64.0f) / std::max(1.0f, view.arena_width),
                                              (height - 240.0f) / std::max(1.0f, view.arena_height)));
    return {scale, (width - view.arena_width * scale) / 2, 110.0f + (height - 240 - view.arena_height * scale) / 2};
}

void paint_arena(HDC dc, int width, int height, const GameView& view,
                 const ArenaHud& hud) {
    const auto& toggles = hud.toggles;
    const bool paused = hud.paused, effects = hud.effects, vectors = hud.vectors;
    if (width <= 0 || height <= 0) return;
    Bitmap buffer(width, height, PixelFormat32bppPARGB);
    Graphics g(&buffer);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color(255, 12, 18, 31));
    const auto v = viewport(width, height, view);
    const float w = view.arena_width * v.scale, h = view.arena_height * v.scale;
    const auto saved = g.Save();
    g.SetClip(RectF(v.left, v.top, w, h));
    g.TranslateTransform(view.camera_offset.x * v.scale, view.camera_offset.y * v.scale);
    SolidBrush floor(Color(255, 20, 31, 47));
    g.FillRectangle(&floor, v.left - 100, v.top - 100, w + 200, h + 200);
    Pen grid(Color(255, 30, 46, 63));
    for (int i = 0; i <= static_cast<int>(view.arena_width); i += 2)
        g.DrawLine(&grid, v.left + i * v.scale, v.top, v.left + i * v.scale, v.top + h);
    for (int i = 0; i <= static_cast<int>(view.arena_height); i += 2)
        g.DrawLine(&grid, v.left, v.top + i * v.scale, v.left + w, v.top + i * v.scale);
    for (const auto& f : view.fighters) fighter(g, f, v);
    if (vectors) {
        Pen motion(Color(255, 255, 228, 109), 2);
        for (const auto& f : view.fighters)
            g.DrawLine(&motion, v.left + f.pos.x * v.scale, v.top + f.pos.y * v.scale,
                       v.left + (f.pos.x + f.motion.x * 10) * v.scale,
                       v.top + (f.pos.y + f.motion.y * 10) * v.scale);
    }
    for (const auto& p : view.popups)
        text(g, std::to_wstring(p.amount), v.left + p.pos.x * v.scale,
             v.top + (p.pos.y - p.age * 2) * v.scale - 28, 22, Color::White);
    if (view.blur.strength > 0) {
        Pen ray(Color(static_cast<BYTE>(std::clamp(view.blur.strength, 0.0f, 1.0f) * 100), 190, 160, 255), 2);
        const float cx = v.left + view.blur.centre.x * v.scale, cy = v.top + view.blur.centre.y * v.scale;
        for (int i = 0; i < 32; ++i) {
            const float a = i * pi / 16, d = std::max(w, h);
            g.DrawLine(&ray, cx + std::cos(a) * d * .25f, cy + std::sin(a) * d * .25f,
                       cx + std::cos(a) * d, cy + std::sin(a) * d);
        }
    }
    g.Restore(saved);
    Pen border(Color(255, 65, 95, 120), 2);
    g.DrawRectangle(&border, v.left, v.top, w, h);
    text(g, L"ACTION LAB", 32, 18, 24, Color(255, 89, 231, 215));
    text(g, L"WAVE " + std::to_wstring(view.wave) + L"    SCORE " + std::to_wstring(view.score) +
            L"    COMBO " + std::to_wstring(view.combo), 250, 22, 18, Color::White);
    if (!view.fighters.empty()) {
        const auto& p = view.fighters.front();
        const float maxhp = static_cast<float>(std::max(1, p.max_hp));
        bar(g, 32, 62, 220, 10, view.player_hp_trail / maxhp, Color(255, 240, 171, 98));
        SolidBrush health(Color(255, 89, 231, 215));
        g.FillRectangle(&health, 32.0f, 62.0f, 220 * std::clamp(p.hp / maxhp, 0.0f, 1.0f), 10.0f);
        text(g, L"HP " + std::to_wstring(p.hp) + L" / " + std::to_wstring(p.max_hp), 268, 56, 15, Color::White);
        const std::string weapon(p.weapon);
        text(g, std::wstring(weapon.begin(), weapon.end()), 430, 56, 15, Color::White);
    }
    const auto rules = [](const SideSettings& s) {
        return std::wstring(L"flinch ") + (s.flinch ? L"ON" : L"OFF") + L" / armor " +
            (s.super_armor == SuperArmor::Off ? L"OFF" : s.super_armor == SuperArmor::Attack ? L"ATTACK" : L"ALWAYS") +
            L" / cancel " + (s.motion_cancel ? L"ON" : L"OFF");
    };
    text(g, L"Enemy: " + rules(hud.settings.enemy) + L"      Player: " + rules(hud.settings.player), 32, 84, 13, Color(255, 161, 183, 203));
    text(g, L"1 HIT STOP " + std::wstring(toggles.hit_stop() ? L"ON" : L"OFF") +
            L"    2 KNOCKBACK " + (toggles.knockback() ? L"ON" : L"OFF") +
            L"    3 SHAKE " + (toggles.camera_shake() ? L"ON" : L"OFF") + L"    0 ALL", 32, height - 108.0f, 16, Color::White);
    text(g, L"WASD move / Mouse aim / Click-J attack / K weapon / P pause / R reset / O effects / V vectors", 32, height - 83.0f, 14, Color(255, 161, 183, 203));
    text(g, L"F1 baseline / F2-4 traps / F5 reload C++ / F6 rollback / 4-6 enemy rules / 7-9 player rules", 32, height - 61.0f, 14, Color(255, 161, 183, 203));
    text(g, hud.message, 32, height - 37.0f, 13, Color(255, 226, 196, 128));
    if (!effects && (paused || view.status != GameStatus::Playing)) {
        const std::wstring label = paused ? L"PAUSED" : view.status == GameStatus::Won ? L"ARENA CLEAR" : L"TRY AGAIN";
        text(g, label, width * .5f - 100, height * .5f - 25, 32, Color::White);
        text(g, paused ? L"P to resume" : L"R to restart", width * .5f - 65, height * .5f + 18, 18, Color::White);
    }
    if (effects) {
        SolidBrush panel(Color(245, 12, 18, 31));
        g.FillRectangle(&panel, width * .5f - 250, 110.0f, 500.0f, 365.0f);
        text(g, L"EFFECTS / A-J toggle / O close", width * .5f - 225, 125, 20, Color::White);
        int row = 0;
        for (const auto& e : effect_table()) {
            const std::wstring name(e.name.begin(), e.name.end());
            text(g, std::wstring(1, wchar_t(L'A' + row)) + L"   " + name + (toggles[e.id] ? L"  ON" : L"  OFF"),
                 width * .5f - 225, 164.0f + row * 27, 17, Color(255, 89, 231, 215));
            ++row;
        }
    }
    Graphics target(dc);
    target.DrawImage(&buffer, 0, 0);
}
} // namespace lesson_action_feel::graphical
