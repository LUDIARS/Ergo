#include "screen.h"

#include "key_bindings.h"
#include "screen_effects.h"

#include <cmath>
#include <cstdio>
#include <string_view>

namespace lesson_action_feel::terminal {

namespace {

class Canvas {
public:
    Canvas(int cols, int rows) : cols_(cols), rows_(rows), cells_(static_cast<std::size_t>(rows), std::string(static_cast<std::size_t>(cols), ' ')) {}
    void put(int col, int row, char ch) {
        if (col < 0 || row < 0 || col >= cols_ || row >= rows_) return;
        cells_[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] = ch;
    }
    char at(int col, int row) const {
        if (col < 0 || row < 0 || col >= cols_ || row >= rows_) return '\0';
        return cells_[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
    }
    void text(int col, int row, std::string_view s) {
        for (std::size_t i = 0; i < s.size(); ++i) put(col + static_cast<int>(i), row, s[i]);
    }
    void centered(int row, std::string_view s) { text((cols_ - static_cast<int>(s.size())) / 2, row, s); }
    int  cols() const { return cols_; }
    std::vector<std::string>& rows() { return cells_; }

private:
    int cols_;
    int rows_;
    std::vector<std::string> cells_;
};

/// Maps world positions to canvas cells, with the camera shake offset.
struct Viewport {
    int ox = 0;   ///< shake, in cells
    int oy = 0;
    int col(float x) const { return kMarginCols + 1 + static_cast<int>(std::floor(x * kColsPerUnit)) + ox; }
    int row(float y) const { return kMarginRows + 1 + static_cast<int>(std::floor(y)) + oy; }
};

const char* on_off(bool b) { return b ? "ON " : "off"; }

/// `v` with `decimals` digits after the point (literal format, checked length).
std::string num(double v, int decimals = 0) {
    char      buf[48];
    const int n = std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    if (n < 0 || n >= static_cast<int>(sizeof(buf))) return "?";
    return buf;
}

/// `s` padded on the left to `width` characters.
std::string pad(std::string s, std::size_t width) {
    if (s.size() < width) s.insert(0, width - s.size(), ' ');
    return s;
}

void draw_walls(Canvas& cv, const Viewport& vp, const GameView& v) {
    const int left   = vp.col(0.0f) - 1;
    const int right  = vp.col(v.arena_width - 0.001f) + 1;
    const int top    = vp.row(0.0f) - 1;
    const int bottom = vp.row(v.arena_height - 0.001f) + 1;
    for (int c = left; c <= right; ++c) {
        cv.put(c, top, '#');
        cv.put(c, bottom, '#');
    }
    for (int r = top; r <= bottom; ++r) {
        cv.put(left, r, '#');
        cv.put(right, r, '#');
    }
}

/// Every arena cell whose centre the fighter's swing covers gets `mark` (unless a body is drawn there later).
void draw_swing(Canvas& cv, const Viewport& vp, const GameView& v, const FighterView& f, char mark) {
    const int cols = static_cast<int>(v.arena_width * kColsPerUnit);
    const int rows = static_cast<int>(v.arena_height);
    for (int cy = 0; cy < rows; ++cy) {
        for (int cx = 0; cx < cols; ++cx) {
            const Vec2  centre{(static_cast<float>(cx) + 0.5f) / kColsPerUnit, static_cast<float>(cy) + 0.5f};
            const Vec2  d    = centre - f.pos;
            const float dist = d.length();
            if (dist > f.reach || dist < 0.25f) continue;   // not the fighter's own cell
            if ((d * (1.0f / dist)).dot(f.facing) < f.arc_cos) continue;
            cv.put(vp.col(centre.x), vp.row(centre.y), mark);
        }
    }
}

char facing_arrow(Vec2 f) {
    if (std::fabs(f.x) >= std::fabs(f.y)) return f.x >= 0 ? '>' : '<';
    return f.y >= 0 ? 'v' : '^';
}

char enemy_glyph(const FighterView& e) {
    switch (e.action) {
    case Action::Dead: return 'x';
    case Action::Flinch: return '~';
    case Action::Attack:
        if (e.phase != AttackPhase::Recovery) return '!';
        break;
    case Action::Ready: break;
    }
    if (e.boss) return 'B';
    return e.armored_now ? 'E' : 'e';
}

/// Put `ch` only on a background cell inside the arena (blur and trails never cover anything).
void put_background(Canvas& cv, const Viewport& vp, const GameView& v, int col, int row, char ch) {
    if (col <= vp.col(0.0f) - 1 || col >= vp.col(v.arena_width - 0.001f) + 1) return;
    if (row <= vp.row(0.0f) - 1 || row >= vp.row(v.arena_height - 0.001f) + 1) return;
    if (cv.at(col, row) == ' ') cv.put(col, row, ch);
}

void draw_boss_blur(Canvas& cv, const Viewport& vp, const GameView& v) {
    if (v.blur.kind == BlurKind::None || v.blur.strength <= 0.0f) return;
    const int cc = vp.col(v.blur.centre.x);
    const int cr = vp.row(v.blur.centre.y);
    for (int r = 0; r < static_cast<int>(cv.rows().size()); ++r)
        for (int c = 0; c < cv.cols(); ++c)
            if (const char g = boss_blur_glyph(c - cc, r - cr, v.blur.strength, v.blur.kind)) put_background(cv, vp, v, c, r, g);
}

void draw_cells(Canvas& cv, const Viewport& vp, const GameView& v, const FighterView& f,
                const std::vector<GlyphCell>& cells) {
    const int c = vp.col(f.pos.x);
    const int r = vp.row(f.pos.y);
    for (const GlyphCell& g : cells) put_background(cv, vp, v, c + g.dc, r + g.dr, g.ch);
}

void draw_fighters(Canvas& cv, const Viewport& vp, const GameView& v) {
    for (const FighterView& f : v.fighters)
        if (f.visible && !f.streak.is_zero()) draw_cells(cv, vp, v, f, trail_cells(f.streak));
    for (const FighterView& f : v.fighters) {
        if (f.action != Action::Attack || f.side == Side::Player || f.roar) continue;
        if (f.phase == AttackPhase::WindUp && f.telegraph) draw_swing(cv, vp, v, f, '.');
        else if (f.phase == AttackPhase::Active) draw_swing(cv, vp, v, f, ':');
    }
    if (!v.fighters.empty()) {
        const FighterView& p = v.fighters.front();
        if (p.action == Action::Attack && p.phase == AttackPhase::Active) draw_swing(cv, vp, v, p, '+');
    }
    for (std::size_t i = 1; i < v.fighters.size(); ++i) {
        const FighterView& e = v.fighters[i];
        if (!e.visible) continue;
        const int c = vp.col(e.pos.x);
        const int r = vp.row(e.pos.y);
        cv.put(c, r, enemy_glyph(e));
        if (e.action != Action::Dead) {
            const std::string hp = std::to_string(e.hp);
            cv.text(c - static_cast<int>(hp.size()) / 2, r - 1, hp);
        }
    }
    if (!v.fighters.empty()) {
        const FighterView& p = v.fighters.front();
        const int c = vp.col(p.pos.x);
        const int r = vp.row(p.pos.y);
        cv.put(c, r, p.action == Action::Flinch ? '&' : (p.action == Action::Dead ? 'x' : '@'));
        if (p.action == Action::Ready) {
            const char a  = facing_arrow(p.facing);
            const int  dc = a == '>' ? 1 : (a == '<' ? -1 : 0);
            const int  dr = a == 'v' ? 1 : (a == '^' ? -1 : 0);
            if (cv.at(c + dc, r + dr) == ' ') cv.put(c + dc, r + dr, a);
        }
    }
    for (const FighterView& f : v.fighters) {
        if (!f.spark || !f.visible) continue;
        const int c = vp.col(f.pos.x);
        const int r = vp.row(f.pos.y);
        cv.put(c - 1, r, '*');
        cv.put(c + 1, r, '*');
    }
}

/// Damage numbers rise one row over their life.
void draw_popups(Canvas& cv, const Viewport& vp, const GameView& v, float life) {
    for (const DamagePopup& p : v.popups) {
        const std::string text = "-" + std::to_string(p.amount);
        const int rise = p.age >= life * 0.5f ? 2 : 1;
        const int c0   = vp.col(p.pos.x) + 2;
        const int r    = vp.row(p.pos.y) - rise;
        for (std::size_t i = 0; i < text.size(); ++i) put_background(cv, vp, v, c0 + static_cast<int>(i), r, text[i]);
    }
}

void draw_vectors(Canvas& cv, const Viewport& vp, const GameView& v) {
    for (const FighterView& f : v.fighters)
        if (f.visible && !f.motion.is_zero()) draw_cells(cv, vp, v, f, vector_cells(f.motion));
}

/// `#` the HP, `=` the part that is still draining (the HP gauge effect), `.` empty.
std::string hp_bar(std::int32_t hp, std::int32_t trail, std::int32_t max_hp) {
    const auto cells = [max_hp](std::int32_t v) { return max_hp > 0 ? static_cast<int>((10 * v + max_hp - 1) / max_hp) : 0; };
    const int n = cells(hp);
    const int t = cells(trail);
    std::string bar = "[";
    for (int i = 0; i < 10; ++i) bar += i < n ? '#' : (i < t ? '=' : '.');
    return bar + "]";
}

/// `s` padded on the right to `width` characters.
std::string left(std::string s, std::size_t width) {
    if (s.size() < width) s.append(width - s.size(), ' ');
    return s;
}

std::string count(std::int64_t n) { return pad(num(static_cast<double>(n)), 9); }

/// One row of the per-side table: the switches with their keys, then what happened.
std::string side_row(const char* who, const SideSettings& s, const SideCounters& n, char k1, char k2, char k3) {
    const auto key = [](char k) { return std::string("[") + k + "] "; };
    return left(who, 8) + left(key(k1) + on_off(s.flinch), 9) + left(key(k2) + super_armor_name(s.super_armor), 14) +
           left(key(k3) + on_off(s.motion_cancel), 14) + "|" + count(n.flinches) + count(n.armor_absorbed) +
           count(n.cancels);
}

void hud_top(std::vector<std::string>& out, const GameView& v, const HudState& h) {
    const FighterView* p = v.fighters.empty() ? nullptr : &v.fighters.front();
    std::string l1 = p ? "HP " + pad(num(p->hp), 3) + "/" + num(p->max_hp) + " " + hp_bar(p->hp, v.player_hp_trail, p->max_hp)
                       : std::string("HP -");
    if (p) {
        std::string weapon = p->weapon;
        for (char& ch : weapon) ch = static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - 'a' + 'A' : ch);
        l1 += "  " + weapon + " dmg " + num(p->weapon_damage) + " pow " + num(p->weapon_power);
    }
    l1 += "  wave " + num(v.wave) + (v.waves_to_win > 0 ? "/" + num(v.waves_to_win) : std::string(" (endless)"));
    l1 += "  KO " + num(static_cast<double>(v.defeated));
    out.push_back(l1);

    std::string l2 = "score " + num(static_cast<double>(v.score)) + "  combo " + num(v.combo) + "  time " +
                     num(v.real_seconds, 1) + "s  world " + num(v.world_seconds, 1) + "s";
    if (v.frozen) l2 += "  HIT STOP";
    for (const FighterView& f : v.fighters)
        if (f.boss && f.action == Action::Attack && f.phase != AttackPhase::Recovery)
            l2 += f.roar ? "  BOSS ROAR" : "  BOSS CHARGE";
    if (v.blur.strength > 0.0f) l2 += "  blur " + num(v.blur.strength, 2);
    out.push_back(l2);
    std::size_t on = 0;
    for (const EffectInfo& i : effect_table()) on += h.toggles[i.id] ? 1 : 0;
    out.push_back(std::string("effects [1]hit stop ") + on_off(h.toggles.hit_stop()) + " [2]knockback " +
                  on_off(h.toggles.knockback()) + " [3]shake " + on_off(h.toggles.camera_shake()) + "  [O]all " +
                  num(static_cast<double>(on)) + "/" + num(static_cast<double>(kEffectCount)) + " on [0]");
    out.push_back("        flinch   super armor   motion cancel |flinched absorbed cancelled");
    out.push_back(side_row("enemy", h.settings.enemy, h.observed.enemy, '4', '5', '6'));
    out.push_back(side_row("you", h.settings.player, h.observed.player, '7', '8', '9'));
}

/// The effects screen (O): every registry entry with its key and state, over the arena.
void effects_screen(Canvas& cv, const HudState& h) {
    const int left = kMarginCols + 3;
    int       row  = kMarginRows + 1;
    cv.text(left, row++, "EFFECTS (paused)  a-j toggle  0 all off/on  O close");
    for (const EffectInfo& i : effect_table()) {
        std::string line = std::string(1, screen_key_for(i.id)) + "  " + std::string(i.name);
        line.resize(18, ' ');
        line += h.toggles[i.id] ? "ON   " : "off  ";
        line += i.gameplay ? "gameplay  " : "visual    ";
        line += "feel." + std::string(i.name);
        line.resize(56, ' ');
        cv.text(left, row++, line);
    }
}

void overlay(Canvas& cv, const GameView& v, const HudState& h) {
    if (h.effects_screen) {
        effects_screen(cv, h);
        return;
    }
    const int mid = kMarginRows + 1 + static_cast<int>(v.arena_height) / 2;
    if (v.status == GameStatus::Won) cv.centered(mid, "  YOU WIN - R to play again, Q to quit  ");
    else if (v.status == GameStatus::Lost) cv.centered(mid, "  GAME OVER - R to retry, Q to quit  ");
    else if (h.paused) cv.centered(mid, "  PAUSED - P to resume  ");
    else if (v.between_waves) cv.centered(mid, "  WAVE " + num(v.wave) + " CLEARED  ");
}

std::string vec_text(Vec2 m) {
    const auto sgn = [](float x) { return std::string(x < 0 ? "" : "+") + num(x, 2); };
    return "(" + sgn(m.x) + "," + sgn(m.y) + ")";
}

} // namespace

std::string vector_line(const GameView& v, int last_hit_id) {
    std::string out = "motion u/frame: you" + (v.fighters.empty() ? std::string("-") : vec_text(v.fighters.front().motion));
    for (const FighterView& f : v.fighters)
        if (f.id == last_hit_id && f.side == Side::Enemy) out += "  hit#" + std::to_string(f.id) + vec_text(f.motion);
    out += "  cam" + vec_text(v.camera_motion);
    return out;
}

std::vector<std::string> render_screen(const GameView& view, const HudState& hud) {
    std::vector<std::string> out;
    hud_top(out, view, hud);

    const int cols = static_cast<int>(view.arena_width * kColsPerUnit) + 2 + 2 * kMarginCols;
    const int rows = static_cast<int>(view.arena_height) + 2 + 2 * kMarginRows;
    Canvas    cv(cols, rows);
    Viewport  vp;
    vp.ox = static_cast<int>(std::lround(view.camera_offset.x * kColsPerUnit));
    vp.oy = static_cast<int>(std::lround(view.camera_offset.y));
    draw_walls(cv, vp, view);
    draw_fighters(cv, vp, view);
    draw_popups(cv, vp, view, hud.popup_seconds);
    draw_boss_blur(cv, vp, view);   // last of the arena layers: it only fills what is still background
    if (hud.debug_vectors) draw_vectors(cv, vp, view);
    overlay(cv, view, hud);
    for (std::string& r : cv.rows()) out.push_back(std::move(r));

    std::string msg = hud.link.empty() ? std::string() : "[" + hud.link + "] ";
    out.push_back(msg + hud.message);
    if (hud.debug_vectors) out.push_back(vector_line(view, hud.last_hit_id));
    else if (hud.show_help) out.emplace_back(key_help());
    for (std::string& r : out)
        if (r.size() > kMaxColumns) r.resize(kMaxColumns);   // a long message must not wrap and push the picture
    return out;
}

} // namespace lesson_action_feel::terminal
