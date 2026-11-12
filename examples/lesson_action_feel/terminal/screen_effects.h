#pragma once
/// lesson_action_feel terminal — how the blur effects become characters. A
/// terminal cannot blur pixels; these are honest stand-ins, pure functions.
///
/// Aspect: a terminal cell is about twice as tall as it is wide, so one world
/// unit is drawn as 2 columns x 1 row (screen.h kColsPerUnit). Every vector in
/// world units is converted with that before it is drawn, so a diagonal motion
/// stays diagonal on screen.
///
/// - Boss blur (one full-screen effect): speed lines on the background around
///   the boss. Radial (roar): lines spreading out from the boss, as far as
///   strength x the radius. Zoom (charge): lines converging on the boss from
///   the outside, reaching closer as the strength grows. Below 0.5 there are 8
///   lines, from 0.5 on 16. Glyphs - | / \ (by the line's direction).
/// - Motion blur (per object): a trail of cells behind the body along its
///   streak, fading with distance: % (near) ; (middle) , (far).
/// - Debug vectors: the motion vector itself, x10 frames, drawn forward from
///   the body as - | / \ with an arrow head > < ^ v at the tip.
/// Only background cells (spaces) are ever drawn over; bodies, numbers and the
/// HUD stay sharp.
#include "game_view.h"
#include "vec2.h"

#include <vector>

namespace lesson_action_feel::terminal {

constexpr float kBlurRadius  = 16.0f;   ///< world units the boss blur can reach from its centre
constexpr float kVectorScale = 10.0f;   ///< debug arrows show 10 frames of motion

/// The glyph of a background cell (dx_cols, dy_rows) away from the blur centre, or 0 for none.
char boss_blur_glyph(int dx_cols, int dy_rows, float strength, BlurKind kind);

struct GlyphCell {
    int  dc = 0;   ///< columns from the body
    int  dr = 0;   ///< rows from the body
    char ch = 0;
    bool operator==(const GlyphCell& o) const { return dc == o.dc && dr == o.dr && ch == o.ch; }
};

/// The motion-blur trail of a body whose streak (world units, pointing backwards) is `streak`.
std::vector<GlyphCell> trail_cells(Vec2 streak);

/// The debug drawing of a motion vector (world units per frame).
std::vector<GlyphCell> vector_cells(Vec2 motion);

} // namespace lesson_action_feel::terminal
