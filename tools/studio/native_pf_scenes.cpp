#include "native_pf_scenes.h"

#include "plugins/common/json_parse.h"

#include <algorithm>

namespace ergo::studio::app {
namespace {
using plugins::JsonValue;
std::string value(const JsonValue& row, const char* key) {
    const auto* v = row.find(key);
    return v ? v->as_string() : std::string();
}
const std::vector<JsonValue>& rows(const JsonValue& row, const char* key) {
    static const std::vector<JsonValue> empty;
    const auto* v = row.find(key);
    return v && v->is_array() ? *v->a : empty;
}
const JsonValue* find_row(const std::vector<JsonValue>& list, const std::string& id) {
    for (const auto& row : list)
        if (value(row, "id") == id) return &row;
    return nullptr;
}
} // namespace
void NativePfScenes::reset() {
    ++generation_;
    catalog_ = detail_ = {};
    layout_.clear();
    spec_.clear();
    note_.clear();
    needs_catalog_ = dirty_ = true;
    needs_detail_ = false;
}
bool NativePfScenes::update(plugins::scene_editor::SceneEditorPlugin& editor) {
    using namespace std::chrono;
    const auto now = steady_clock::now();
    if (request_.valid() && request_.wait_for(seconds(0)) == std::future_status::ready) {
        const auto response = request_.get();
        if (requested_generation_ == generation_ &&
            (requested_spec_.empty() || requested_spec_ == spec_)) {
            auto data = plugins::parse_json(response.body);
            const bool ok = response.status == 200 && data;
            note_ = ok ? "" : (data ? value(*data, "err") : "Pf の応答を読み取れません");
            if (requested_spec_.empty()) {
                if (value(catalog_, "projectId") != (ok ? value(*data, "projectId") : "")) {
                    layout_.clear();
                    spec_.clear();
                    detail_ = {};
                }
                catalog_ = ok ? std::move(*data) : JsonValue{};
                const auto& scenes = rows(catalog_, "scenes");
                if (!find_row(scenes, layout_) && !scenes.empty()) {
                    layout_ = value(scenes.front(), "id");
                    for (const auto& row : scenes)
                        if (row.find("isDefault") && row.find("isDefault")->as_bool()) {
                            layout_ = value(row, "id");
                            break;
                        }
                }
                const auto* scene = find_row(scenes, layout_);
                if (!scene) layout_.clear();
                if (!scene || !find_row(rows(*scene, "specs"), spec_)) {
                    spec_.clear();
                    detail_ = {};
                }
                needs_detail_ = !spec_.empty();
                next_ = now + seconds(30);
            } else
                detail_ = ok ? std::move(*data) : JsonValue{};
            dirty_ = true;
        }
    }
    if (!request_.valid() && (needs_catalog_ || now >= next_ || needs_detail_)) {
        const bool catalog = needs_catalog_ || now >= next_;
        if (catalog)
            needs_catalog_ = false;
        else
            needs_detail_ = false;
        requested_spec_ = catalog ? "" : spec_;
        requested_generation_ = generation_;
        auto path = std::string("/api/praeforma");
        if (!catalog) path += "/" + value(catalog_, "projectId") + "/specs/" + spec_;
        request_ = std::async(std::launch::async, [&editor, path] {
            net::HttpRequest req;
            req.method = "GET";
            return editor.handle(req, path);
        });
    }
    const bool changed = dirty_;
    dirty_ = false;
    return changed;
}
std::optional<std::string> NativePfScenes::runtime(
    const std::vector<plugins::scene_editor::SceneInfo>& scenes) const {
    if (layout_.empty()) return std::nullopt;
    const auto* bindings = catalog_.find("bindings");
    for (const auto& s : scenes) {
        const auto* app = bindings ? bindings->find(s.app) : nullptr;
        const auto* mapped = app ? app->find(s.id) : nullptr;
        if ((mapped ? mapped->as_string() : s.id) == layout_) return s.id;
    }
    return std::string{};
}
void NativePfScenes::panel(::tela::Document& doc, ::tela::Layout bounds) {
    if (!expanded_) {
        bounds.y += bounds.height - 36;
        bounds.height = 36;
        doc.button("pf/open", "Pf シーン・仕様", [this] { expanded_ = dirty_ = true; }, bounds);
        return;
    }
    doc.scroll(
        "pf/panel",
        [&] {
            doc.button("pf/close", "Pf シーン・仕様を閉じる", [this] {
                expanded_ = false;
                dirty_ = true;
            });
            doc.button("pf/refresh", "Pf から更新", [this] { needs_catalog_ = true; });
            if (!note_.empty()) doc.text("pf/note", note_);
            const auto& scenes = rows(catalog_, "scenes");
            if (scenes.empty() && note_.empty())
                doc.text("pf/empty", request_.valid() ? "読み込み中" : "Pf にシーンがありません");
            for (const auto& row : scenes) {
                const auto id = value(row, "id");
                doc.button("pf/layout/" + id, (layout_ == id ? "✓ " : "") + value(row, "name"),
                           [this, id] {
                               layout_ = id;
                               spec_.clear();
                               detail_ = {};
                               needs_detail_ = false;
                               dirty_ = true;
                           });
            }
            const auto* scene = find_row(scenes, layout_);
            if (!scene) return;
            doc.text("pf/description", value(*scene, "description"));
            const auto& specs = rows(*scene, "specs");
            if (specs.empty()) doc.text("pf/no-specs", "このシーンに紐づく仕様はありません");
            for (const auto& s : specs) {
                const auto id = value(s, "id");
                doc.button("pf/spec/" + id,
                           value(s, "code") + " " + value(s, "title") + " " + value(s, "status"),
                           [this, id] {
                               spec_ = id;
                               detail_ = {};
                               needs_detail_ = dirty_ = true;
                           });
            }
            if (const auto* spec = detail_.find("spec")) {
                doc.text("pf/title", value(*spec, "title"));
                if (const auto* version = spec->find("version"))
                    doc.text("pf/version", "版 " + plugins::to_json(*version));
                doc.text("pf/body", value(*spec, "description"));
                size_t index = 0;
                for (const auto& item : rows(detail_, "acceptance"))
                    doc.text("pf/accept/" + std::to_string(index++), value(item, "text"));
            }
        },
        bounds);
}
} // namespace ergo::studio::app
