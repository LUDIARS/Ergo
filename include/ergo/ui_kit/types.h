#pragma once

/// ergo_ui_kit — 共通型 (幾何 / スタイル / レイアウト指定)。
/// GPU / Pictor 依存は持たない。

#include <cstdint>

namespace ergo::ui_kit {

struct Vec2  { float x = 0.0f, y = 0.0f; };
struct Rect  { float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f; };
struct Color { float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f; };

/// ノード種別 (Figma のノード種別に対応)。
enum class NodeKind : uint8_t {
    Frame     = 0,   // コンテナ。 auto-layout / clip / 背景塗り
    Text      = 1,   // 文字列
    Image     = 2,   // テクスチャ矩形
    Shape     = 3,   // 塗り + ストローク + 角丸
    NineSlice = 4,   // 可変サイズ枠 (9-slice)
};

/// auto-layout の方向。 None なら子は constraints で個別配置。
enum class LayoutMode : uint8_t { None = 0, Horizontal = 1, Vertical = 2 };

/// auto-layout 内でのノードのサイズ決定。
///   Fixed = 宣言サイズ / Hug = 中身に合わせる / Fill = 余白を埋める
enum class Sizing : uint8_t { Fixed = 0, Hug = 1, Fill = 2 };

/// auto-layout の主軸 / 交差軸の整列。
enum class AlignMain  : uint8_t { Start = 0, Center = 1, End = 2, SpaceBetween = 3 };
enum class AlignCross : uint8_t { Start = 0, Center = 1, End = 2, Stretch = 3 };

/// constraints (親が auto-layout でないときのピン留め)。
enum class Pin : uint8_t { Start = 0, Center = 1, End = 2, Stretch = 3, Scale = 4 };

/// テキスト整列。
enum class TextAlign : uint8_t { Left = 0, Center = 1, Right = 2 };

/// レイアウト指定 (宣言値)。
struct LayoutSpec {
    LayoutMode mode = LayoutMode::None;
    float      gap   = 0.0f;
    float      pad_l = 0.0f, pad_t = 0.0f, pad_r = 0.0f, pad_b = 0.0f;
    AlignMain  align_main  = AlignMain::Start;
    AlignCross align_cross = AlignCross::Start;
    Sizing     w_sizing = Sizing::Fixed;
    Sizing     h_sizing = Sizing::Fixed;
    // 親が auto-layout でないときの自ノードのピン留め (constraints)。
    Pin        pin_x = Pin::Start;
    Pin        pin_y = Pin::Start;
};

/// 見た目スタイル。
struct Style {
    Color    fill          = {0, 0, 0, 0};   // 背景塗り (a=0 で無描画)
    Color    stroke        = {0, 0, 0, 0};
    float    stroke_width  = 0.0f;
    float    corner_radius = 0.0f;
    Color    text_color    = {1, 1, 1, 1};
    float    font_size     = 16.0f;
    TextAlign text_align   = TextAlign::Left;
    uint32_t texture_id    = 0;              // Image / NineSlice (ホストが用意)
    float    nine_l = 0, nine_t = 0, nine_r = 0, nine_b = 0;  // 9-slice insets
};

} // namespace ergo::ui_kit
