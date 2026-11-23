#pragma once
/// Draws a ViewNode tree with ergo_ui_kit (the built-in window of a build
/// without Tela). Click handlers are recorded by node path, which StudioApp
/// wires into ui_kit's event dispatch; ui_kit does not measure text, so Text
/// nodes get their size from the ITextMeasure.
#include "ergo/studio/ui/view_builder.h"

#include <string>
#include <vector>

namespace ergo::studio::ui {

struct ClickBinding {
    std::string path;   ///< ui_kit resolved path of the clickable node
    ClickFn     fn;
};

struct SliderBinding {
    std::string   path;   ///< resolved path of the track
    SetFractionFn fn;
};

/// A text field: the string it edits lives in the view (it must outlive the
/// next rebuild), the app routes typed text to the focused one.
struct FieldBinding {
    std::string  path;        ///< resolved path of the field's frame
    std::string* value = nullptr;
    ClickFn      on_submit;   ///< Enter (single-line fields only)
    bool         multiline = false;
};

/// What one build of the UI registers. `focused` is kept by the app across
/// rebuilds (the path of the field that receives keys, empty = none).
struct Bindings {
    std::vector<ClickBinding>  clicks;
    std::vector<FieldBinding>  fields;
    std::vector<SliderBinding> sliders;
    std::string                focused;
    void clear() {
        clicks.clear();
        fields.clear();
        sliders.clear();
    }
};

/// Append the children of `tree` under `out`, whose resolved path is `out_path`.
/// `tree`'s own container settings (fixed width, fill, alignment) apply to `out`.
void emit_ui_kit(const ViewNode& tree, ui_kit::Node& out, const std::string& out_path, const ITextMeasure& measure,
                 const Theme& theme, Bindings& bindings);

} // namespace ergo::studio::ui
