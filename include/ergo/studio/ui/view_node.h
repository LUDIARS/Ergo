#pragma once
/// What a plugin tab declares, independent of who draws it.
///
/// A ViewBuilder fills a tree of ViewNodes; a backend turns it into its own
/// form: ui_kit nodes for the built-in window (ui_kit_emitter.h) or a
/// tela::Document for the Tela window and the browser (ergo/studio/tela/).
/// Values a control edits stay with the view (`value` points into it); the
/// tree only describes one build and is thrown away on the next.
#include "ergo/ui_kit/ui_kit.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ergo::studio::ui {

using ClickFn = std::function<void()>;
/// A slider: where along the track the user set it (0 = left .. 1 = right).
using SetFractionFn = std::function<void(float)>;

enum class ViewKind : uint8_t {
    Group,       ///< the container a builder starts from (no look of its own)
    Row,         ///< children left to right
    Panel,       ///< children top to bottom on a panel background
    Text,        ///< one line of text
    Button,
    Field,       ///< text input (one line, secret or several lines)
    ListItem,    ///< full-width clickable line
    Slider,
    Status,      ///< good / bad mark + label
    Separator,
    Spacer,
    Video,       ///< a media stream (the browser shows it; others show the frame)
};

struct ViewNode {
    ViewKind    kind = ViewKind::Group;
    std::string name;     ///< unique among its siblings ("label3"); the path of names is the stable id
    std::string text;     ///< label / button text / list item / status / field placeholder / video label
    std::string detail;   ///< list item detail, video stream key

    ui_kit::Color color{};      ///< Text colour
    float         font_size = 0;   ///< Text size in px
    float         width     = 0;   ///< fixed width (fields, sliders, video, fixed containers); 0 = natural
    float         height    = 0;   ///< spacer / video height
    float         gap       = 0;   ///< Row / Panel spacing
    float         fraction  = 0;   ///< Slider position, 0..1
    int           lines     = 1;   ///< Field rows

    bool primary   = false;   ///< Button
    bool selected  = false;   ///< ListItem
    bool ok        = false;   ///< Status
    bool multiline = false;   ///< Field: Enter inserts a newline
    bool secret    = false;   ///< Field: a dot per character
    bool align_start = false; ///< Row: children keep their top edge (default: centred)
    bool has_fill  = false;   ///< Panel: `fill` instead of the theme's panel colour
    ui_kit::Color fill{};

    std::string*  value = nullptr;   ///< Field: the string the view keeps
    ClickFn       on_click;          ///< Button / ListItem
    ClickFn       on_submit;         ///< Field: Enter (one-line fields)
    SetFractionFn on_set;            ///< Slider

    std::vector<ViewNode> children;
};

} // namespace ergo::studio::ui
