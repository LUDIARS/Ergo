#pragma once
/// Declares a ViewNode tree as Tela elements (SPEC-TL-DECLARATION / CONTROLS).
///
/// Ids are the path of node names under `prefix` ("content/studio/panel0/button2"),
/// so caret, scroll and pressed state survive rebuilds. A Tela row gives a child
/// of width 0 the rest of the row, so children of a row get their natural width
/// (text measured with the ITextMeasure). Tela has one text colour: labels in the
/// theme's ok / warn / error colour become status lines with that tone, and
/// align_start / fill of a container are not drawn (one panel colour).
#include "ergo/studio/ui/view_builder.h"

#include <tela/document.hpp>

#include <functional>
#include <string>

namespace ergo::studio::tela_shell {

struct EmitOptions {
    const ui::ITextMeasure& measure;
    const ui::Theme&        theme;
    /// False: buttons, list items, fields and sliders are declared without
    /// actions (a screen that must not operate this content).
    bool interactive = true;
    /// Runs after every action, edit or slider move (the declaration rebuilds).
    std::function<void()> touched;
    bool published = false;
};

/// Declare the children of `tree` under the element being declared now.
void emit_children(const ui::ViewNode& tree, ::tela::Document& doc, const std::string& prefix, const EmitOptions& o);

/// Width a node takes in a row (logical px).
float natural_width(const ui::ViewNode& node, const EmitOptions& o);

/// The Studio theme in Tela's terms.
::tela::Theme tela_theme(const ui::Theme& theme);
::tela::Color tela_color(const ui_kit::Color& c);

/// ui::ITextMeasure with Tela's font-less estimate (0.55 em ASCII, 1 em otherwise),
/// so rows leave room for wide (CJK) text in every screen.
class ApproximateMeasure final : public ui::ITextMeasure {
public:
    float width(const std::string& utf8, float font_size) const override;
    float line_height(float font_size) const override { return font_size * 1.35f; }
};

} // namespace ergo::studio::tela_shell
