#include "lookdev_view.h"

#include "json_render.h"
#include "../plugins/variable/variable_plugin.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace ergo::studio::views {
namespace jm = ergo::common::jsonm;
using plugins::variable::BoundVar;

namespace {

constexpr const char* kLookPrefix = "look/";
constexpr const char* kCaptureGroup = "Capture";

std::string category_of(const BoundVar& v) {
    const jm::JsonValue* c = v.meta.find("category");
    return c != nullptr ? c->as_string() : std::string();
}

bool is_look(const BoundVar& v) { return category_of(v).rfind(kLookPrefix, 0) == 0; }

std::string group_of(const BoundVar& v) { return category_of(v).substr(std::string(kLookPrefix).size()); }

double meta_number(const BoundVar& v, const char* key, double fallback) {
    const jm::JsonValue* n = v.meta.find(key);
    return n != nullptr && n->is_number() ? n->n : fallback;
}

/// "look.ssao_radius_m" -> "ssao_radius_m".
std::string short_name(const std::string& name) {
    const auto dot = name.find('.');
    return dot == std::string::npos ? name : name.substr(dot + 1);
}

std::string number_text(double value) {
    char text[32];
    std::snprintf(text, sizeof(text), "%.4g", value);
    return text;
}

std::string value_text(const jm::JsonValue& v) {
    if (v.is_bool()) return v.b ? "true" : "false";
    if (v.is_number()) return number_text(v.n);
    if (v.is_string()) return v.s;
    if (v.is_array() && v.a) {
        std::string out;
        for (std::size_t i = 0; i < v.a->size(); ++i) out += (i ? "/" : "") + number_text((*v.a)[i].as_number());
        return out;
    }
    return "-";
}

const BoundVar* find_var(const std::vector<BoundVar>& vars, const std::string& name) {
    for (const auto& v : vars) {
        if (v.name == name) return &v;
    }
    return nullptr;
}

} // namespace

LookdevView::LookdevView(host::IPlugin& plugin, host::IPlugin* variable)
    : ExplorerView(plugin), variable_(dynamic_cast<plugins::variable::VariablePlugin*>(variable)) {}

uint64_t LookdevView::view_revision() const {
    // The game's edits arrive through the variable plugin: its changes redraw this tab too.
    return ApiView::view_revision() + (variable_ != nullptr ? variable_->revision() : 0);
}

void LookdevView::on_first_show() {
    get("health", "/api/health");
    get("presets", "/api/presets");
}

void LookdevView::set(const std::string& name, const JsonValue& value) {
    if (variable_ == nullptr || !variable_->request_set(name, value)) {
        set_notice("\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0\xE3\x81\xAB\xE5\xB1\x8A\xE3\x81\x8D\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93\xE3\x81\xA7\xE3\x81\x97\xE3\x81\x9F: " + name, false);   // ゲームに届きませんでした: 
    }
    touch();
}

void LookdevView::remember_baseline(const std::vector<BoundVar>& vars) {
    // A new connection (another app name, or the first) starts a new baseline.
    const BoundVar* any = nullptr;
    for (const auto& v : vars) {
        if (is_look(v)) any = &v;
    }
    if (any == nullptr) {
        baseline_.clear();
        app_.clear();
        return;
    }
    if (any->app != app_) {
        baseline_.clear();
        app_ = any->app;
    }
    for (const auto& v : vars) {
        if (is_look(v) && group_of(v) != kCaptureGroup && !baseline_.count(v.name)) {
            baseline_[v.name] = jm::serialize(v.value);
        }
    }
}

std::vector<std::string> LookdevView::changes(const std::vector<BoundVar>& vars) const {
    std::vector<std::string> out;
    for (const auto& v : vars) {
        if (!is_look(v) || group_of(v) == kCaptureGroup) continue;
        const auto base = baseline_.find(v.name);
        if (base == baseline_.end() || base->second == jm::serialize(v.value)) continue;
        jm::JsonValue before;
        jm::parse(base->second, before);
        out.push_back(short_name(v.name) + ": " + value_text(before) + " \xE2\x86\x92 " + value_text(v.value));   //  → 
    }
    return out;
}

void LookdevView::start_capture() {
    if (variable_ == nullptr || capturing_) return;
    fetch("capture_dir", "POST", "/api/capture-dir", "{}", [this](const ApiResult& r) {
        if (!r.ok()) {
            set_notice(r.error_text(), false);
            return;
        }
        const jm::JsonValue* dir = r.json.find("dir");
        if (dir == nullptr || !dir->is_string()) {
            set_notice("\xE6\x92\xAE\xE5\xBD\xB1\xE3\x83\x95\xE3\x82\xA9\xE3\x83\xAB\xE3\x83\x80\xE3\x82\x92\xE4\xBD\x9C\xE3\x82\x8C\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93\xE3\x81\xA7\xE3\x81\x97\xE3\x81\x9F", false);   // 撮影フォルダを作れませんでした
            return;
        }
        std::string previous;
        for (const auto& v : variable_->vars()) {
            if (v.name == "look.capture_path") previous = v.value.as_string();
        }
        previous_capture_ = previous;
        capturing_ = true;
        if (!variable_->request_set("look.capture_request", jm::JsonValue::make_string(dir->s))) {
            capturing_ = false;
            set_notice("\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0\xE3\x81\x8C\xE6\x8E\xA5\xE7\xB6\x9A\xE3\x81\x97\xE3\x81\xA6\xE3\x81\x84\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93 (\xE6\x92\xAE\xE5\xBD\xB1\xE3\x82\x92\xE9\xA0\xBC\xE3\x82\x81\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93\xE3\x81\xA7\xE3\x81\x97\xE3\x81\x9F)", false);   // ゲームが接続していません (撮影を頼めませんでした)
            return;
        }
        set_notice("\xE6\x92\xAE\xE5\xBD\xB1\xE4\xB8\xAD\xE2\x80\xA6", true);   // 撮影中…
    });
}

void LookdevView::check_capture(const std::vector<BoundVar>& vars) {
    if (!capturing_) return;
    const BoundVar* path = find_var(vars, "look.capture_path");
    if (path == nullptr || path->value.as_string().empty() || path->value.as_string() == previous_capture_) return;
    capturing_ = false;
    jm::JsonValue body = jm::JsonValue::make_object();
    body.set("png", jm::JsonValue::make_string(path->value.as_string()));
    body.set("comment", jm::JsonValue::make_string(comment_));
    body.set("app", jm::JsonValue::make_string(app_));
    jm::JsonValue list = jm::JsonValue::make_array();
    for (const auto& change : changes(vars)) list.push(jm::JsonValue::make_string(change));
    body.set("changes", std::move(list));
    set_notice("\xE6\x8A\x95\xE7\xA8\xBF\xE4\xB8\xAD\xE2\x80\xA6", true);   // 投稿中…
    fetch("post", "POST", "/api/post", jm::serialize(body), [this](const ApiResult& r) {
        set_notice(r.ok() ? std::string("Discord / Slack \xE3\x81\xAB\xE6\x8A\x95\xE7\xA8\xBF\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F") : r.error_text(), r.ok());   // Discord / Slack に投稿しました
    });
}

void LookdevView::save_preset(const std::vector<BoundVar>& vars) {
    const BoundVar* preset = find_var(vars, "look.preset");
    if (preset == nullptr || preset_name_.empty()) {
        set_notice(preset == nullptr ? "\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0\xE3\x81\x8C\xE6\x8E\xA5\xE7\xB6\x9A\xE3\x81\x97\xE3\x81\xA6\xE3\x81\x84\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93" : "\xE3\x83\x97\xE3\x83\xAA\xE3\x82\xBB\xE3\x83\x83\xE3\x83\x88\xE5\x90\x8D\xE3\x81\x8C\xE8\xA6\x81\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x99", false);   // ゲームが接続していません / プリセット名が要ります
        return;
    }
    jm::JsonValue body = jm::JsonValue::make_object();
    body.set("name", jm::JsonValue::make_string(preset_name_));
    body.set("text", jm::JsonValue::make_string(preset->value.as_string()));
    fetch("save", "POST", "/api/presets", jm::serialize(body), [this](const ApiResult& r) {
        set_notice(r.ok() ? std::string("\xE4\xBF\x9D\xE5\xAD\x98\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F: ") + preset_name_ : r.error_text(), r.ok());   // 保存しました: 
        if (r.ok()) get("presets", "/api/presets");
    });
}

void LookdevView::load_preset(const std::string& name) {
    fetch("load", "GET", "/api/presets/" + name, {}, [this, name](const ApiResult& r) {
        const jm::JsonValue* text = r.json.find("text");
        if (!r.ok() || text == nullptr) {
            set_notice(r.ok() ? std::string("\xE8\xAA\xAD\xE3\x82\x81\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93\xE3\x81\xA7\xE3\x81\x97\xE3\x81\x9F: ") + name : r.error_text(), false);   // 読めませんでした: 
            return;
        }
        if (variable_ == nullptr || !variable_->request_set("look.apply", *text)) {
            set_notice("\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0\xE3\x81\x8C\xE6\x8E\xA5\xE7\xB6\x9A\xE3\x81\x97\xE3\x81\xA6\xE3\x81\x84\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93", false);   // ゲームが接続していません
            return;
        }
        edits_.clear();
        set_notice("\xE8\xAA\xAD\xE3\x81\xBF\xE8\xBE\xBC\xE3\x81\xBF\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F: " + name, true);   // 読み込みました: 
    });
}

void LookdevView::render(ui::ViewBuilder& b) {
    b.title(plugin_.info().title);
    b.muted(plugin_.info().description);
    {
        const Slot& h = slot("health");
        if (h.loaded && !(h.result.json.find("ok") && h.result.json.find("ok")->as_bool())) {
            b.label(text_of(h.result.json, "err"), &b.theme().warn);
        }
    }
    render_notice(b);
    if (variable_ == nullptr) {
        b.label("variable \xE3\x83\x97\xE3\x83\xA9\xE3\x82\xB0\xE3\x82\xA4\xE3\x83\xB3\xE3\x81\x8C\xE3\x81\x82\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93 (\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0\xE3\x81\xAE\xE5\x80\xA4\xE3\x82\x92\xE8\xAA\xAD\xE3\x82\x81\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93)", &b.theme().error);   // variable プラグインがありません (ゲームの値を読めません)
        return;
    }
    const std::vector<BoundVar> vars = variable_->vars();
    remember_baseline(vars);
    check_capture(vars);
    if (app_.empty()) {
        b.paragraph("\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0\xE3\x81\x8C\xE6\x8E\xA5\xE7\xB6\x9A\xE3\x81\x97\xE3\x81\xA6\xE3\x81\x84\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93\xE3\x80\x82\n\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0\xE3\x82\x92\xE3\x83\xAB\xE3\x83\x83\xE3\x82\xAF\xE3\x83\x87\xE3\x83\xB4\xE4\xBB\x98\xE3\x81\x8D\xE3\x81\xA7\xE8\xB5\xB7\xE5\x8B\x95\xE3\x81\x97\xE3\x81\xA6\xE3\x81\x8F\xE3\x81\xA0\xE3\x81\x95\xE3\x81\x84 (Amicii: amicii_client ... --lookdev 5170)\xE3\x80\x82");   // ゲームが接続していません。\nゲームをルックデヴ付きで起動してください (Amicii: amicii_client ... --lookdev 5170)。
        return;
    }
    b.field("\xE6\x8E\xA5\xE7\xB6\x9A\xE4\xB8\xAD", app_);   // 接続中
    render_actions(b, vars);
    render_group(b, vars);
}

void LookdevView::render_actions(ui::ViewBuilder& b, const std::vector<BoundVar>& vars) {
    ui::ViewBuilder panel = b.panel(5);
    {
        ui::ViewBuilder r = panel.row(6);
        r.text_field(comment_, "\xE3\x82\xB3\xE3\x83\xA1\xE3\x83\xB3\xE3\x83\x88 (\xE6\x8A\x95\xE7\xA8\xBF\xE3\x81\xAB\xE6\xB7\xBB\xE3\x81\x88\xE3\x82\x8B)", 420);   // コメント (投稿に添える)
        r.button(capturing_ ? "\xE6\x92\xAE\xE5\xBD\xB1\xE4\xB8\xAD\xE2\x80\xA6" : "\xE6\x92\xAE\xE5\xBD\xB1\xE3\x81\x97\xE3\x81\xA6\xE6\x8A\x95\xE7\xA8\xBF", [this] { start_capture(); }, true);   // 撮影中… / 撮影して投稿
    }
    {
        ui::ViewBuilder r = panel.row(6);
        r.text_field(preset_name_, "\xE3\x83\x97\xE3\x83\xAA\xE3\x82\xBB\xE3\x83\x83\xE3\x83\x88\xE5\x90\x8D", 200);   // プリセット名
        r.button("\xE3\x83\x97\xE3\x83\xAA\xE3\x82\xBB\xE3\x83\x83\xE3\x83\x88\xE4\xBF\x9D\xE5\xAD\x98", [this, vars] { save_preset(vars); });   // プリセット保存
        const std::size_t changed = changes(vars).size();
        r.muted("\xE6\x8E\xA5\xE7\xB6\x9A\xE6\x99\x82\xE3\x81\x8B\xE3\x82\x89\xE5\xA4\x89\xE3\x81\x88\xE3\x81\x9F\xE9\xA0\x85\xE7\x9B\xAE: " + std::to_string(changed));   // 接続時から変えた項目: 
    }
    const Slot& presets = slot("presets");
    if (presets.loaded && presets.result.json.is_array() && !presets.result.json.a->empty()) {
        ui::ViewBuilder r = panel.row(6);
        r.muted("\xE8\xAA\xAD\xE3\x81\xBF\xE8\xBE\xBC\xE3\x82\x80:");   // 読み込む:
        std::size_t shown = 0;
        for (const auto& p : *presets.result.json.a) {
            if (++shown > 8) break;
            const std::string name = text_of(p, "name");
            r.button(name, [this, name] { load_preset(name); });
        }
    }
}

void LookdevView::render_group(ui::ViewBuilder& b, const std::vector<BoundVar>& vars) {
    std::vector<std::string> groups;
    for (const auto& v : vars) {
        if (!is_look(v) || group_of(v) == kCaptureGroup) continue;
        if (std::find(groups.begin(), groups.end(), group_of(v)) == groups.end()) groups.push_back(group_of(v));
    }
    if (groups.empty()) return;
    if (std::find(groups.begin(), groups.end(), group_) == groups.end()) group_ = groups.front();
    {
        ui::ViewBuilder tabs = b.row(4);
        for (const auto& g : groups) {
            tabs.button(g, [this, g] {
                group_ = g;
                touch();
            }, g == group_);
        }
    }
    ui::ViewBuilder panel = b.panel(6);
    for (const auto& v : vars) {
        if (!is_look(v) || group_of(v) != group_) continue;
        if (v.kind == "bool") {
            const bool on = v.value.as_bool();
            ui::ViewBuilder r = panel.row(8);
            r.label(short_name(v.name));
            const std::string name = v.name;
            r.button(on ? "ON" : "OFF", [this, name, on] { set(name, jm::JsonValue::make_bool(!on)); }, on);
        } else if (v.kind == "color" || v.kind == "vec3") {
            render_vector(panel, v);
        } else if (v.kind != "string") {
            render_number(panel, v);
        }
    }
}

void LookdevView::render_number(ui::ViewBuilder& b, const BoundVar& v) {
    const double lo = meta_number(v, "min", 0.0);
    const double hi = meta_number(v, "max", 0.0);
    const bool integer = v.kind == "int32" || v.kind == "int64";
    double step = meta_number(v, "step", 0.0);
    if (step <= 0.0) step = hi > lo ? (hi - lo) / 100.0 : 0.01;
    const double value = v.value.as_number();
    const std::string name = v.name;
    auto clamp_set = [this, name, lo, hi, integer](double x) {
        if (hi > lo) x = std::min(hi, std::max(lo, x));
        if (integer) x = std::round(x);
        set(name, jm::JsonValue::make_number(x));
    };
    ui::ViewBuilder r = b.row(8);
    r.clipped_label(short_name(v.name), 190);
    if (hi > lo) {
        r.slider(static_cast<float>((value - lo) / (hi - lo)), [clamp_set, lo, hi](float f) { clamp_set(lo + (hi - lo) * f); },
                 260);
    }
    r.button("-", [clamp_set, value, step] { clamp_set(value - step); });
    r.button("+", [clamp_set, value, step] { clamp_set(value + step); });
    std::string& text = edits_[name];
    if (text.empty()) text = number_text(value);
    r.text_field(text, number_text(value), 90, [this, clamp_set, name] {
        std::string& t = edits_[name];
        char* end = nullptr;
        const double x = std::strtod(t.c_str(), &end);
        if (end != t.c_str()) clamp_set(x);
        t.clear();
    });
    r.muted(number_text(value));
}

void LookdevView::render_vector(ui::ViewBuilder& b, const BoundVar& v) {
    if (!v.value.is_array() || !v.value.a) return;
    const std::vector<jm::JsonValue> parts = *v.value.a;
    const double lo = meta_number(v, "min", 0.0);
    const double hi = meta_number(v, "max", 1.0) > lo ? meta_number(v, "max", 1.0) : lo + 1.0;
    static const char* kAxis[4] = {"r", "g", "b", "w"};
    b.label(short_name(v.name));
    for (std::size_t i = 0; i < parts.size() && i < 4; ++i) {
        ui::ViewBuilder r = b.row(8);
        r.muted(std::string("  ") + kAxis[i]);
        const double value = parts[i].as_number();
        const std::string name = v.name;
        r.slider(static_cast<float>((value - lo) / (hi - lo)),
                 [this, name, parts, i, lo, hi](float f) {
                     jm::JsonValue next = jm::JsonValue::make_array();
                     for (std::size_t k = 0; k < parts.size(); ++k) {
                         next.push(jm::JsonValue::make_number(k == i ? lo + (hi - lo) * f : parts[k].as_number()));
                     }
                     set(name, next);
                 },
                 260);
        r.muted(number_text(value));
    }
}

} // namespace ergo::studio::views
