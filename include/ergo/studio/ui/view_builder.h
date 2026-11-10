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

class ViewBuilder {
public:
    /// Appends under `container`, whose resolved path will be `container_path`.
    ViewBuilder(ui_kit::Node& container, std::string container_path, const ITextMeasure& measure, const Theme& theme,
                std::vector<ClickBinding>& clicks);

    /// The node this builder appends to (resolved again on every call:
    /// children live in vectors, so a reference would dangle after a sibling
    /// is added higher up).
    ui_kit::Node& node();

    const Theme& theme() const { return theme_; }

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

    /// A horizontal row; `fill` makes it take the full width.
    ViewBuilder row(float gap = 8);
    /// A vertical group with a panel background.
    ViewBuilder panel(float gap = 6);

private:
    ViewBuilder(ui_kit::Node& root, std::vector<std::size_t> index, std::string path, const ITextMeasure& measure,
                const Theme& theme, std::vector<ClickBinding>& clicks);
    ViewBuilder   child(ui_kit::Node n);   ///< add `n` and return a builder for it
    void          add(ui_kit::Node n);
    std::string   next_name(const char* kind);

    ui_kit::Node&              root_;
    std::vector<std::size_t>   index_;   ///< child indices from root_
    std::string                path_;
    const ITextMeasure&        measure_;
    const Theme&               theme_;
    std::vector<ClickBinding>& clicks_;
    int                        counter_ = 0;
};

} // namespace ergo::studio::ui
