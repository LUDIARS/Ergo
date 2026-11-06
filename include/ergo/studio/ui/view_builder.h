#pragma once
/// Building a plugin tab as an ergo_ui_kit node tree (GPU independent).
///
/// Views are rebuilt whenever their data changes (IPluginView::revision());
/// a ViewBuilder appends nodes to a container and records click handlers by
/// node path, which the Studio app wires into ui_kit's event dispatch.
/// ui_kit does not measure text, so Text nodes get their size from an
/// ITextMeasure (the shell's font in the app, a fixed advance in tests).
#include "ergo/ui_kit/ui_kit.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ergo::studio::ui {

class ITextMeasure {
public:
    virtual ~ITextMeasure() = default;
    virtual float width(const std::string& utf8, float font_size) const = 0;
    virtual float line_height(float font_size) const = 0;
};

/// Monospace estimate: every code point is `advance * font_size` wide.
class FixedAdvanceMeasure final : public ITextMeasure {
public:
    explicit FixedAdvanceMeasure(float advance = 0.55f) : advance_(advance) {}
    float width(const std::string& utf8, float font_size) const override;
    float line_height(float font_size) const override { return font_size * 1.3f; }

private:
    float advance_;
};

/// Cut `text` so that it fits `max_width` (whole code points, "…" appended).
std::string fit_text(const std::string& text, float max_width, float font_size, const ITextMeasure& m);
/// Break `text` into lines no wider than `max_width` (greedy; prefers spaces).
std::vector<std::string> wrap_text(const std::string& text, float max_width, float font_size, const ITextMeasure& m);
/// Keep the tail of `text` that fits `max_width` (a field shows where you type).
std::string fit_tail(const std::string& text, float max_width, float font_size, const ITextMeasure& m);

/// Colours of the Studio (one dark theme).
struct Theme {
    ui_kit::Color background{0.11f, 0.12f, 0.14f, 1};
    ui_kit::Color panel{0.15f, 0.16f, 0.19f, 1};
    ui_kit::Color panel_alt{0.18f, 0.19f, 0.23f, 1};
    ui_kit::Color accent{0.30f, 0.55f, 0.95f, 1};
    ui_kit::Color text{0.92f, 0.93f, 0.95f, 1};
    ui_kit::Color muted{0.62f, 0.65f, 0.70f, 1};
    ui_kit::Color ok{0.35f, 0.80f, 0.45f, 1};
    ui_kit::Color warn{0.95f, 0.75f, 0.30f, 1};
    ui_kit::Color error{0.95f, 0.40f, 0.40f, 1};
    float         font_size  = 15.0f;
    float         small_size = 13.0f;
    float         title_size = 20.0f;
};

using ClickFn = std::function<void()>;

struct ClickBinding {
    std::string path;   ///< ui_kit resolved path of the clickable node
    ClickFn     fn;
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
    std::vector<ClickBinding> clicks;
    std::vector<FieldBinding> fields;
    std::string               focused;
    void clear() {
        clicks.clear();
        fields.clear();
    }
};

class ViewBuilder {
public:
    /// Appends under `container`, whose resolved path will be `container_path`.
    ViewBuilder(ui_kit::Node& container, std::string container_path, const ITextMeasure& measure, const Theme& theme,
                Bindings& bindings);

    /// The node this builder appends to (resolved again on every call:
    /// children live in vectors, so a reference would dangle after a sibling
    /// is added higher up).
    ui_kit::Node& node();

    const Theme&        theme() const { return theme_; }
    const ITextMeasure& measure() const { return measure_; }

    void title(const std::string& text);
    void label(const std::string& text, const ui_kit::Color* color = nullptr, float font_size = 0);
    void muted(const std::string& text);
    /// One line per "\n"; long lines are not wrapped (a scroll view clips them).
    void paragraph(const std::string& text);
    void button(const std::string& text, ClickFn on_click, bool primary = false);
    /// A key / value line ("clients: 2").
    void field(const std::string& key, const std::string& value, const ui_kit::Color* value_color = nullptr);
    /// Coloured dot + label (health).
    void status(bool ok, const std::string& text);
    void separator();
    void spacer(float h);
    /// One-line text input editing `value` (kept by the caller). Enter runs
    /// `on_submit`. The width is fixed; long text shows its tail.
    void text_field(std::string& value, const std::string& placeholder = {}, float width = 320, ClickFn on_submit = {});
    /// A one-line input that shows a dot per character (tokens, passwords).
    void secret_field(std::string& value, const std::string& placeholder = {}, float width = 320);
    /// Multi-line input, `lines` rows high (Enter inserts a newline).
    void text_area(std::string& value, float width, int lines);
    /// A full-width clickable line (lists: files, issues, tasks...).
    void list_item(const std::string& text, bool selected, ClickFn on_click, const std::string& detail = {});
    /// Text wrapped to `max_width` px (at spaces, or anywhere for CJK); "\n" starts a new line.
    void wrapped(const std::string& text, float max_width, const ui_kit::Color* color = nullptr, float font_size = 0);
    /// A label cut to `max_width` px with "…" (table cells).
    void clipped_label(const std::string& text, float max_width, const ui_kit::Color* color = nullptr);

    /// A horizontal row; `fill` makes it take the full width.
    ViewBuilder row(float gap = 8);
    /// A vertical group with a panel background.
    ViewBuilder panel(float gap = 6);

private:
    ViewBuilder(ui_kit::Node& root, std::vector<std::size_t> index, std::string path, const ITextMeasure& measure,
                const Theme& theme, Bindings& bindings);
    void          field_frame(std::string& value, const std::string& placeholder, float width, int lines, bool multiline,
                              ClickFn on_submit, bool masked = false);
    ViewBuilder   child(ui_kit::Node n);   ///< add `n` and return a builder for it
    void          add(ui_kit::Node n);
    std::string   next_name(const char* kind);

    ui_kit::Node&              root_;
    std::vector<std::size_t>   index_;   ///< child indices from root_
    std::string                path_;
    const ITextMeasure&        measure_;
    const Theme&               theme_;
    Bindings&                  bind_;
    int                        counter_ = 0;
};

} // namespace ergo::studio::ui
