#pragma once
/// Building a plugin tab as a ViewNode tree (GPU and backend independent).
///
/// Views are rebuilt whenever their data changes (IPluginView::revision());
/// a ViewBuilder appends ViewNodes to a container. A backend draws the tree:
/// ui_kit (ui_kit_emitter.h) for the built-in window, Tela (ergo/studio/tela/)
/// for the Tela window and the browser. Text that must fit a width (wrapped,
/// clipped_label) is cut here with an ITextMeasure (the shell's font in the
/// app, a fixed advance in tests and for Tela).
#include "ergo/studio/ui/view_node.h"

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

class ViewBuilder {
public:
    /// Appends to `container` (kept by the caller until the tree is drawn).
    ViewBuilder(ViewNode& container, const ITextMeasure& measure, const Theme& theme);

    /// The container this builder appends to (resolved again on every call:
    /// children live in vectors, so a reference would dangle after a sibling
    /// is added higher up). Views adjust its `width`, `align_start` or `fill`.
    ViewNode& node();

    const Theme&        theme() const { return theme_; }
    const ITextMeasure& measure() const { return measure_; }

    void title(const std::string& text);
    void label(const std::string& text, const ui_kit::Color* color = nullptr, float font_size = 0);
    void muted(const std::string& text);
    /// One line per "\n"; long lines are not wrapped (a scroll view clips them).
    void paragraph(const std::string& text);
    void button(const std::string& text, ClickFn on_click, bool primary = false, bool local_only = false);
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
    /// A horizontal track filled to `fraction` (0..1); a click sets the fraction under the pointer.
    void slider(float fraction, SetFractionFn on_set, float width = 240);
    /// Text wrapped to `max_width` px (at spaces, or anywhere for CJK); "\n" starts a new line.
    void wrapped(const std::string& text, float max_width, const ui_kit::Color* color = nullptr, float font_size = 0);
    /// A label cut to `max_width` px with "…" (table cells).
    void clipped_label(const std::string& text, float max_width, const ui_kit::Color* color = nullptr);
    /// A media stream `stream` (a key the browser page resolves, e.g. "play:<target>"; empty = none
    /// yet). Hosts that cannot show it draw the frame and `label`.
    void video(const std::string& label, const std::string& stream, float width, float height);
    /// The next element gets `name` instead of a counted one, so page scripts and tests can find it
    /// (unique among its siblings; [A-Za-z0-9_-] only).
    void name_next(std::string name);

    /// A horizontal row.
    ViewBuilder row(float gap = 8);
    /// A vertical group with a panel background.
    ViewBuilder panel(float gap = 6);

private:
    ViewBuilder(ViewNode& root, std::vector<std::size_t> index, const ITextMeasure& measure, const Theme& theme);
    ViewBuilder child(ViewNode n);   ///< add `n` and return a builder for it
    void        add(ViewNode n);
    std::string next_name(const char* kind);
    ViewNode    text_node(const char* kind, const std::string& text, float size, const ui_kit::Color& color);

    ViewNode&                root_;
    std::vector<std::size_t> index_;   ///< child indices from root_
    const ITextMeasure&      measure_;
    const Theme&             theme_;
    int                      counter_ = 0;
    std::string              pending_name_;
};

} // namespace ergo::studio::ui
