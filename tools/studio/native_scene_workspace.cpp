#include "native_scene_workspace.h"

#include "plugins/common/json_parse.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace ergo::studio::app {
namespace {
::tela::Layout box(float x, float y, float w, float h) {
    ::tela::Layout l;
    l.positioned = true;
    l.x = x;
    l.y = y;
    l.width = w;
    l.height = h;
    l.padding = 0;
    return l;
}
} // namespace
NativeSceneWorkspace::NativeSceneWorkspace(host::PluginHost& plugins, project::OpenProject& project)
    : project_(project) {
    for (auto* p : plugins.plugins()) {
        if (p->info().id == "scene_editor")
            editor_ = dynamic_cast<plugins::scene_editor::SceneEditorPlugin*>(p);
        if (p->info().id == "play") play_ = p->http();
    }
}
std::string NativeSceneWorkspace::game_window() const {
    if (!open_ || active_target_.empty()) return {};
    for (const auto& t : targets_)
        if (t.name == active_target_ && t.scene == selected_) return t.window;
    return {};
}
void NativeSceneWorkspace::preview_status(const std::string& message) {
    if (!active_target_.empty() && !message.empty() && message != play_note_) {
        play_note_ = message;
        dirty_ = true;
    }
}
bool NativeSceneWorkspace::update(::tela::Runtime& runtime,
                                  const tela_shell::StudioDeclaration& decl) {
    if (!editor_) return false;
    if (project_version_ != project_.revision()) {
        project_version_ = project_.revision();
        const auto current = project_.current();
        const std::string source = current ? current->source : std::string();
        if (source != project_source_) {
            pf_.reset();
            active_target_.clear();
            variant_.clear();
            play_note_.clear();
            variants_open_ = false;
            if (!source.empty()) load_targets();
            project_source_ = source;
            open_ = !source.empty();
            selected_.clear();
            scenes_.clear();
            scene_version_ = ~0ull;
            geometry_dirty_ = true;
            dirty_ = true;
            image_.reset();
        }
    }
    if (project_source_.empty()) return false;
    const auto& vp = runtime.viewport();
    if (!vp.visible || vp.width <= 0 || vp.height <= 0) return true;
    const auto dpi = std::max(.1f, vp.dpi_scale);
    const auto width = std::max(1, int(vp.width / dpi)), height = std::max(1, int(vp.height / dpi));
    if (width != width_ || height != height_) {
        width_ = width;
        height_ = height;
        geometry_dirty_ = true;
        dirty_ = true;
    }
    if (!open_) {
        if (dirty_ || launcher_version_ != decl.revision()) {
            launcher_version_ = decl.revision();
            auto doc = decl.document(tela_shell::ScreenAccess::window);
            doc.button(
                "window/open-scene", "シーンビューを開く",
                [this] {
                    open_ = true;
                    dirty_ = true;
                },
                box(std::max(0, width_ - 166), 8, 158, 34));
            runtime.theme(decl.theme());
            runtime.document(std::move(doc));
            dirty_ = false;
        }
        return true;
    }
    const auto version = editor_->revision();
    if (version != scene_version_) {
        scenes_ = editor_->scenes();
        scene_version_ = version;
        geometry_dirty_ = true;
        dirty_ = true;
    }
    if (pf_.update(*editor_)) dirty_ = true;
    if (auto match = pf_.runtime(scenes_); match && *match != selected_) {
        selected_ = *match;
        geometry_dirty_ = dirty_ = true;
        image_.reset();
    }
    if (pending_.valid() &&
        pending_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        auto ready = pending_.get();
        if (pending_project_ == project_source_ && pending_scene_ == selected_) {
            raster_ = std::move(ready);
            image_.reset();
        }
        dirty_ = true;
    }
    if (!dirty_) return true;
    ::tela::Document doc;
    declare_workspace(doc, float(width_), float(height_));
    runtime.theme(decl.theme());
    runtime.document(std::move(doc));
    dirty_ = false;
    return true;
}
void NativeSceneWorkspace::load_targets() {
    targets_.clear();
    if (!play_) return;
    net::HttpRequest req;
    req.method = "GET";
    const auto res = play_->handle(req, "/api/targets");
    const auto data = plugins::parse_json(res.body);
    const auto* list = data ? data->find("targets") : nullptr;
    if (!list || !list->is_array()) return;
    std::set<std::string> used, ambiguous;
    for (const auto& t : *list->a) {
        const auto* name = t.find("name");
        if (!name || !name->is_string()) continue;
        if (!used.insert(name->s).second) ambiguous.insert(name->s);
        const auto* preview = t.find("scenePreview");
        if (!preview || !preview->is_object()) continue;
        auto field = [&](const char* key) {
            const auto* v = preview->find(key);
            return v ? v->as_string() : std::string();
        };
        targets_.push_back({name->s, field("app"), field("scene"), field("mode"), field("state"),
                            t.find("window") ? t.find("window")->as_string() : std::string()});
    }
    targets_.erase(std::remove_if(targets_.begin(), targets_.end(),
                                  [&](const auto& t) { return ambiguous.count(t.name); }),
                   targets_.end());
}
void NativeSceneWorkspace::run_target(const std::string& name, const std::string& action) {
    if (!play_) return;
    net::HttpRequest req;
    req.method = "POST";
    req.body = "{}";
    const auto res = play_->handle(req, "/api/targets/" + name + "/" + action);
    if (res.status >= 200 && res.status < 300) {
        active_target_ = action == "run" ? name : "";
        play_note_ = action == "run" ? "ゲームのウィンドウを表示します" : "停止を受け付けました";
    } else {
        const auto data = plugins::parse_json(res.body);
        const auto* error = data ? data->find("err") : nullptr;
        play_note_ = error ? error->as_string() : "ゲーム操作を受け付けられませんでした";
        if (action == "stop" && res.status == 409) active_target_.clear();
    }
    dirty_ = true;
}
void NativeSceneWorkspace::declare_play(::tela::Document& doc,
                                        const plugins::scene_editor::SceneInfo& scene,
                                        float width) {
    std::vector<Target> drivers, lights;
    for (const auto& t : targets_) {
        if (t.scene != scene.id || t.app != scene.app) continue;
        if (t.mode == "driver") drivers.push_back(t);
        if (t.mode == "lighting") lights.push_back(t);
    }
    const float w = std::min(252.f, std::max(1.f, width - 16));
    doc.panel(
        "scene/play-tools",
        [&] {
            if (!active_target_.empty()) {
                doc.button("scene/stop", "停止", [this] { run_target(active_target_, "stop"); });
            } else {
                if (drivers.size() == 1)
                    doc.button("scene/run", "再生",
                               [this, name = drivers.front().name] { run_target(name, "run"); });
                if (lights.size() > 0) {
                    const auto it = std::find_if(lights.begin(), lights.end(),
                                                 [&](const auto& t) { return t.name == variant_; });
                    const auto& target = it == lights.end() ? lights.front() : *it;
                    if (lights.size() > 1) {
                        doc.button("scene/state",
                                   "進行状態  " +
                                       (target.state.empty() ? target.name : target.state) + " ▾",
                                   [this] {
                                       variants_open_ = !variants_open_;
                                       dirty_ = true;
                                   });
                        if (variants_open_)
                            for (const auto& t : lights)
                                doc.button("scene/state/" + t.name,
                                           t.state.empty() ? t.name : t.state,
                                           [this, name = t.name] {
                                               variant_ = name;
                                               variants_open_ = false;
                                               dirty_ = true;
                                           });
                    }
                    doc.button("scene/lighting", "ライティング反映",
                               [this, name = target.name] { run_target(name, "run"); });
                }
                if (drivers.size() != 1 || lights.empty())
                    doc.text("scene/capabilities", drivers.size() != 1
                                                       ? "再生用ドライバを一つ登録してください"
                                                       : "ライティング対象が未登録です");
            }
            if (!play_note_.empty()) doc.text("scene/play-note", play_note_);
        },
        box(std::max(8.f, width - w - 8), width < 540 ? 146.f : 8.f, w, 0));
}

void NativeSceneWorkspace::declare_workspace(::tela::Document& doc, float width, float height) {
    auto current = std::find_if(scenes_.begin(), scenes_.end(),
                                [&](const auto& scene) { return scene.id == selected_; });
    if (current == scenes_.end() && !pf_.runtime(scenes_)) current = scenes_.begin();
    if (current != scenes_.end()) {
        if (selected_ != current->id || pending_project_ != project_source_ ||
            pending_scene_ != current->id) {
            raster_ = {};
            raster_.notice = "シーンを描画しています";
            image_.reset();
        }
        selected_ = current->id;
        if (geometry_dirty_ && !pending_.valid()) {
            pending_scene_ = selected_;
            pending_project_ = project_source_;
            pending_ =
                std::async(std::launch::async, [snapshot = *current, w = width_, h = height_] {
                    return raster_scene(snapshot, w, h);
                });
            geometry_dirty_ = false;
            image_.reset();
        }
        if (raster_.notice.empty()) {
            if (!image_) image_ = raster_.image(intensity_);
            ::tela::Drawing drawing;
            drawing.image({0, 0, width, height}, image_);
            doc.canvas("scene/camera", std::move(drawing), box(0, 0, width, height));
        } else
            doc.status("scene/message", raster_.notice, ::tela::Tone::warning,
                       box(8, height * .45f, std::max(1.f, width - 16), 36));
    } else
        doc.text("scene/empty", pf_.runtime(scenes_) ? "この Pf シーンのゲーム接続を待っています" : "シーンカメラの接続を待っています",
                 box(8, height * .45f, std::max(1.f, width - 16), 36));

    const float panel_width = std::min(260.f, std::max(1.f, width - 16));
    doc.panel(
        "scene/tools",
        [&] {
            doc.button("scene/launcher", "ランチャーへ戻る", [this] {
                open_ = false;
                dirty_ = true;
            });
            if (current != scenes_.end()) {
                doc.text("scene/name", current->app + " / " + current->id);
                if (scenes_.size() > 1 && !pf_.runtime(scenes_)) {
                    const auto index = static_cast<std::size_t>(current - scenes_.begin());
                    const auto next = scenes_[(index + 1) % scenes_.size()].id;
                    doc.button("scene/next", "次のシーン", [this, next] {
                        selected_ = next;
                        geometry_dirty_ = true;
                        dirty_ = true;
                    });
                }
            }
        },
        box(8, 8, panel_width, 0));
    const float pf_height = std::min(320.f, height * .45f);
    pf_.panel(doc, box(8, height - pf_height - 8, std::min(340.f, width - 16), pf_height));
    if (current != scenes_.end())
        declare_play(doc, *current, width);
    else if (!active_target_.empty())
        declare_play(doc, {}, width);
    if (active_target_.empty() && current != scenes_.end() && raster_.notice.empty()) {
        doc.panel(
            "scene/light",
            [&] {
                doc.text("scene/light/label", "ライトの強さ");
                doc.slider("scene/light/value", intensity_ * .5f, [this](float value) {
                    intensity_ = 2 * std::clamp(value, 0.f, 1.f);
                    image_.reset();
                    dirty_ = true;
                });
            },
            box(std::max(8.f, width - panel_width - 8), std::max(8.f, height - 94), panel_width,
                0));
    }
}
} // namespace ergo::studio::app
