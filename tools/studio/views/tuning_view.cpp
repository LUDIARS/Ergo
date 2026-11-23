#include "tuning_view.h"

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

double meta_number(const BoundVar& v, const char* key, double fallback) {
    const jm::JsonValue* f = v.meta.find(key);
    return f != nullptr && f->is_number() ? f->as_number() : fallback;
}

std::string category_of(const BoundVar& v) {
    const jm::JsonValue* f = v.meta.find("category");
    return f != nullptr && f->is_string() && !f->as_string().empty() ? f->as_string() : "(未分類)";
}

bool read_only(const BoundVar& v) {
    const jm::JsonValue* f = v.meta.find("read_only");
    return f != nullptr && f->as_bool();
}

bool tunable(const BoundVar& v) {
    return !read_only(v) && (v.kind == "bool" || v.kind == "int32" || v.kind == "int64" || v.kind == "float" ||
                             v.kind == "double" || v.kind == "color" || v.kind == "vec3");
}

std::string short_name(const std::string& name) {
    const std::size_t dot = name.find('.');
    return dot == std::string::npos ? name : name.substr(dot + 1);
}

std::string number_text(double x, bool integer) {
    char buf[64];
    if (integer) std::snprintf(buf, sizeof(buf), "%.0f", x);
    else std::snprintf(buf, sizeof(buf), "%.4g", x);
    return buf;
}

} // namespace

TuningView::TuningView(host::IPlugin& plugin, host::IPlugin* variable)
    : ApiView(plugin), variable_(dynamic_cast<plugins::variable::VariablePlugin*>(variable)) {}

uint64_t TuningView::view_revision() const {
    // Redraw when the game's values move as well as on our own slots.
    return ApiView::view_revision() * 1000003ULL + (variable_ != nullptr ? variable_->revision() : 0);
}

void TuningView::on_first_show() {
    get("health", "/api/health");
    load_versions();
}

void TuningView::load_versions() {
    if (!app_.empty()) get("versions", "/api/versions?app=" + app_);
}

void TuningView::set(const std::string& name, const jm::JsonValue& value) {
    if (variable_ != nullptr && !variable_->request_set(name, value)) set_notice(name + " は今は変えられません", false);
}

void TuningView::render(ui::ViewBuilder& b) {
    b.title(plugin_.info().title);
    b.muted(plugin_.info().description);
    render_notice(b);
    if (variable_ == nullptr) {
        b.label("variable プラグインがありません (ゲームの値を読めません)", &b.theme().error);
        return;
    }
    const std::vector<BoundVar> vars = variable_->vars();
    std::vector<std::string> apps;
    for (const auto& v : vars) {
        if (std::find(apps.begin(), apps.end(), v.app) == apps.end()) apps.push_back(v.app);
    }
    if (apps.empty()) {
        b.paragraph("ゲームが接続していません。ergo_bind 付きで起動してください (Amicii: サーバーは --tune 5170、クライアントは --lookdev 5170)。");
        return;
    }
    if (std::find(apps.begin(), apps.end(), app_) == apps.end()) {
        app_ = apps.front();
        group_.clear();
        load_versions();
    }
    {
        ui::ViewBuilder tabs = b.row(4);
        for (const auto& a : apps) {
            tabs.button(a, [this, a] {
                app_ = a;
                group_.clear();
                compared_.clear();
                load_versions();
                touch();
            }, a == app_);
        }
    }
    render_values(b, vars);
    b.separator();
    render_versions(b, vars);
}

void TuningView::render_values(ui::ViewBuilder& b, const std::vector<BoundVar>& vars) {
    std::vector<std::string> groups;
    for (const auto& v : vars) {
        if (v.app != app_ || !tunable(v)) continue;
        if (std::find(groups.begin(), groups.end(), category_of(v)) == groups.end()) groups.push_back(category_of(v));
    }
    std::sort(groups.begin(), groups.end());
    if (groups.empty()) {
        b.muted("調整できる値がありません");
        return;
    }
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
    ui::ViewBuilder panel = b.panel(5);
    for (const auto& v : vars) {
        if (v.app != app_ || !tunable(v) || category_of(v) != group_) continue;
        if (v.kind == "bool") {
            const bool on = v.value.as_bool();
            ui::ViewBuilder r = panel.row(8);
            r.clipped_label(short_name(v.name), 260);
            const std::string name = v.name;
            r.button(on ? "ON" : "OFF", [this, name, on] { set(name, jm::JsonValue::make_bool(!on)); }, on);
        } else if (v.kind == "color" || v.kind == "vec3") {
            ui::ViewBuilder r = panel.row(8);
            r.clipped_label(short_name(v.name), 260);
            r.label(jm::serialize(v.value));
        } else {
            render_number(panel, v);
        }
    }
}

void TuningView::render_number(ui::ViewBuilder& b, const BoundVar& v) {
    const double lo = meta_number(v, "min", 0.0);
    const double hi = meta_number(v, "max", 0.0);
    const bool integer = v.kind == "int32" || v.kind == "int64";
    const double value = v.value.as_number();
    const std::string name = v.name;
    auto clamp_set = [this, name, lo, hi, integer](double x) {
        if (hi > lo) x = std::min(hi, std::max(lo, x));
        if (integer) x = std::round(x);
        set(name, jm::JsonValue::make_number(x));
    };
    ui::ViewBuilder r = b.row(8);
    r.clipped_label(short_name(v.name), 260);
    // A wide range (a cooldown in ticks, a distance in mm) is dragged on a slider; exact values are typed.
    if (hi > lo) r.slider(static_cast<float>((value - lo) / (hi - lo)), [clamp_set, lo, hi](float f) { clamp_set(lo + (hi - lo) * f); }, 200);
    std::string& text = edits_[name];
    if (text.empty()) text = number_text(value, integer);
    r.text_field(text, {}, 110, [this, clamp_set, name] {
        char* end = nullptr;
        const double x = std::strtod(edits_[name].c_str(), &end);
        if (end != edits_[name].c_str()) clamp_set(x);
        edits_.erase(name);   // show the game's answer
    });
    if (const jm::JsonValue* unit = v.meta.find("unit"); unit != nullptr && unit->is_string()) r.muted(unit->as_string());
}

void TuningView::save_version(const std::vector<BoundVar>& vars) {
    jm::JsonValue values = jm::JsonValue::make_object();
    for (const auto& v : vars) {
        if (v.app == app_ && tunable(v)) values.set(v.name, v.value);
    }
    jm::JsonValue body = jm::JsonValue::make_object();
    body.set("app", jm::JsonValue::make_string(app_));
    body.set("name", jm::JsonValue::make_string(version_name_));
    body.set("note", jm::JsonValue::make_string(version_note_));
    body.set("values", values);
    fetch("save", "POST", "/api/versions", jm::serialize(body), [this](const ApiResult& r) {
        set_notice(r.ok() ? "版を保存しました: " + text_of(r.json, "id") : r.error_text(), r.ok());
        if (r.ok()) {
            version_name_.clear();
            version_note_.clear();
        }
        load_versions();
    });
}

void TuningView::open_version(const std::string& id, bool restore) {
    compared_ = id;
    fetch("version", "GET", "/api/versions/" + app_ + "/" + id, {}, [this, restore, id](const ApiResult& r) {
        if (!r.ok() || !restore) return;
        const jm::JsonValue* values = r.json.find("values");
        if (values == nullptr || !values->is_object()) return;
        std::size_t applied = 0, missing = 0;
        for (const auto& [name, value] : *values->o) {
            if (variable_ != nullptr && variable_->request_set(name, value)) ++applied;
            else ++missing;
        }
        set_notice(id + " を戻しました (" + std::to_string(applied) + " 項目" +
                       (missing > 0 ? "、今のゲームに無い " + std::to_string(missing) + " 項目は飛ばしました" : "") + ")",
                   missing == 0);
    });
}

void TuningView::render_versions(ui::ViewBuilder& b, const std::vector<BoundVar>& vars) {
    b.label("版");
    {
        ui::ViewBuilder r = b.row(6);
        r.text_field(version_name_, "名前", 200);
        r.text_field(version_note_, "メモ", 360);
        r.button("今の値を保存", [this, vars] { save_version(vars); }, true);
    }
    if (!slot_state(b, "versions")) return;
    const auto& answer = slot("versions").result.json;
    b.muted("保存先: " + text_of(answer, "folder"));
    const jm::JsonValue* list = answer.find("versions");
    if (list == nullptr || !list->is_array() || list->a->empty()) {
        b.muted("まだ版がありません");
        return;
    }
    ui::ViewBuilder panel = b.panel(3);
    for (const auto& v : *list->a) {
        const std::string id = text_of(v, "id");
        ui::ViewBuilder r = panel.row(8);
        r.clipped_label(text_of(v, "name") + "  " + text_of(v, "created"), 320);
        r.clipped_label(text_of(v, "note"), 260, &b.theme().muted);
        r.button("差分", [this, id] { open_version(id, false); }, id == compared_);
        r.button("戻す", [this, id] { open_version(id, true); });
    }
    if (compared_.empty()) return;
    const Slot& s = slot("version");
    if (!s.loaded || !s.result.ok()) return;
    const jm::JsonValue* values = s.result.json.find("values");
    if (values == nullptr || !values->is_object()) return;
    b.label(compared_ + " と今の値の違い");
    std::size_t shown = 0;
    for (const auto& v : vars) {
        if (v.app != app_) continue;
        const jm::JsonValue* old = values->find(v.name);
        if (old == nullptr || jm::serialize(*old) == jm::serialize(v.value)) continue;
        b.label(short_name(v.name) + ":  " + jm::serialize(*old) + "  →  " + jm::serialize(v.value) + " (今)");
        ++shown;
    }
    if (shown == 0) b.muted("違いはありません");
}

} // namespace ergo::studio::views
