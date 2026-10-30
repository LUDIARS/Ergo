#include "ergo/scene/editor_link.h"

#include "ergo/common/json_min.h"
#include "ergo/common/ws_client.h"

#include "patch_queue.h"

#include <algorithm>
#include <mutex>
#include <utility>
#include <vector>

namespace ergo::scene {

namespace {

namespace jsonm = ::ergo::common::jsonm;
namespace ws    = ::ergo::common::ws;

std::string hello_frame(const std::string& app) {
    auto m = jsonm::JsonValue::make_object();
    m.set("op",   jsonm::JsonValue::make_string("hello"));
    m.set("role", jsonm::JsonValue::make_string("engine"));
    m.set("app",  jsonm::JsonValue::make_string(app));
    return jsonm::serialize(m);
}

std::string scene_frame(const Scene& scene) {
    auto m = jsonm::JsonValue::make_object();
    m.set("op",   jsonm::JsonValue::make_string("scene"));
    m.set("id",   jsonm::JsonValue::make_string(scene.id()));
    m.set("json", jsonm::JsonValue::make_string(scene.to_json()));
    return jsonm::serialize(m);
}

std::string remove_frame(const std::string& scene_id) {
    auto m = jsonm::JsonValue::make_object();
    m.set("op", jsonm::JsonValue::make_string("scene_remove"));
    m.set("id", jsonm::JsonValue::make_string(scene_id));
    return jsonm::serialize(m);
}

std::string string_field(const jsonm::JsonValue& obj, const char* key) {
    const jsonm::JsonValue* v = obj.find(key);
    return v ? v->as_string() : std::string();
}

} // namespace

// ===========================================================================
// Impl — owns the transport and the published-scene cache. Inbound patches
// are buffered in detail::PatchQueue until the host's poll().
// ===========================================================================
struct EditorLink::Impl {
    // Guards app / published / hook, and serializes outgoing frames so a
    // reconnect's re-send can never overtake a newer publish().
    mutable std::mutex mtx;
    std::string        app = "anonymous";
    // Scene id -> its latest `scene` frame, in publish order.
    std::vector<std::pair<std::string, std::string>> published;
    SendHook           hook;

    detail::PatchQueue patches;

    bool               started = false;  // host thread only
    ws::Client         client;           // last: destroyed (and stopped) first

    void send_locked(const std::string& frame) {
        if (hook) hook(frame);
        else      client.send_text(frame);
    }

    std::vector<std::pair<std::string, std::string>>::iterator find_locked(const std::string& id) {
        return std::find_if(published.begin(), published.end(),
                            [&](const auto& e) { return e.first == id; });
    }

    bool is_published(const std::string& id) {
        std::lock_guard<std::mutex> lk(mtx);
        return find_locked(id) != published.end();
    }

    // Connection (re)opened: worker thread, or set_transport().
    void announce() {
        std::lock_guard<std::mutex> lk(mtx);
        send_locked(hello_frame(app));
        for (const auto& e : published) send_locked(e.second);
    }

    bool publish(const Scene& scene) {
        if (scene.id().empty()) return false;
        std::string frame = scene_frame(scene);
        std::lock_guard<std::mutex> lk(mtx);
        auto it = find_locked(scene.id());
        if (it == published.end()) {
            published.emplace_back(scene.id(), frame);
        } else {
            it->second = frame;
        }
        send_locked(frame);
        return true;
    }

    // Worker thread (or the caller of receive()).
    bool on_text(const std::string& text) {
        jsonm::JsonValue msg;
        if (!jsonm::parse(text, msg) || !msg.is_object()) return false;
        if (string_field(msg, "op") != "patch") return false;
        detail::Patch p;
        p.scene_id = string_field(msg, "scene_id");
        p.actor_id = string_field(msg, "actor_id");
        p.field    = string_field(msg, "field");
        const jsonm::JsonValue* value = msg.find("value");
        if (p.scene_id.empty() || p.actor_id.empty() || p.field.empty() || !value) return false;
        // The relay only forwards patches for scenes this engine published;
        // anything else is stale (unpublished meanwhile) and never polled.
        if (!is_published(p.scene_id)) return false;
        p.value_json = jsonm::serialize(*value);
        patches.push(std::move(p));
        return true;
    }

    // Host thread. `resolve` maps a scene id to the scene to patch (or null).
    template <class Resolve>
    std::size_t apply(Resolve&& resolve, bool keep_unresolved) {
        std::vector<detail::Patch> keep;
        std::vector<Scene*>        touched;
        std::size_t                applied = 0;
        for (detail::Patch& p : patches.drain()) {
            Scene* scene = resolve(p.scene_id);
            if (!scene) {
                if (keep_unresolved && is_published(p.scene_id)) keep.push_back(std::move(p));
                continue;
            }
            if (scene->apply_patch(p.actor_id, p.field, p.value_json)) ++applied;
            if (std::find(touched.begin(), touched.end(), scene) == touched.end()) {
                touched.push_back(scene);
            }
        }
        patches.requeue_front(std::move(keep));
        // Re-publish even when every patch on a scene was rejected: the
        // editor already shows the edited value and must snap back.
        for (Scene* scene : touched) {
            if (is_published(scene->id())) publish(*scene);
        }
        return applied;
    }
};

EditorLink::EditorLink() : impl_(std::make_unique<Impl>()) {
    Impl* impl = impl_.get();
    impl_->client.set_on_open([impl] { impl->announce(); });
    impl_->client.set_on_text([impl](const std::string& t) { impl->on_text(t); });
}

EditorLink::~EditorLink() { disconnect(); }

bool EditorLink::connect(const std::string& host, uint16_t port, const std::string& path) {
    if (impl_->started || host.empty() || port == 0) return false;
    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        if (impl_->hook) return false;
    }
    impl_->started = true;
    impl_->client.start(host, port, path.empty() ? std::string("/scene_editor/ws") : path);
    return true;
}

void EditorLink::disconnect() {
    if (!impl_->started) return;
    impl_->client.stop();
    impl_->started = false;
}

bool EditorLink::is_connected() const {
    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        if (impl_->hook) return true;
    }
    return impl_->client.is_connected();
}

void EditorLink::set_app_name(std::string app) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->app = std::move(app);
}

std::string EditorLink::app_name() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    return impl_->app;
}

bool EditorLink::publish(const Scene& scene) { return impl_->publish(scene); }

bool EditorLink::unpublish(const std::string& scene_id) {
    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        auto it = impl_->find_locked(scene_id);
        if (it == impl_->published.end()) return false;
        impl_->published.erase(it);
        impl_->send_locked(remove_frame(scene_id));
    }
    impl_->patches.discard(scene_id);
    return true;
}

std::size_t EditorLink::poll(SceneGraph& graph) {
    return impl_->apply([&](const std::string& id) { return graph.find_scene(id); },
                        /*keep_unresolved=*/false);
}

std::size_t EditorLink::poll(Scene& scene) {
    return impl_->apply([&](const std::string& id) { return id == scene.id() ? &scene : nullptr; },
                        /*keep_unresolved=*/true);
}

void EditorLink::set_transport(SendHook hook) {
    // The hook replaces the socket; never feed both.
    if (hook) disconnect();
    {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        impl_->hook = std::move(hook);
        if (!impl_->hook) return;
    }
    impl_->announce();
}

bool EditorLink::receive(const std::string& frame) { return impl_->on_text(frame); }

} // namespace ergo::scene
