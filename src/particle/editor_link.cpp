#include "ergo/particle/editor_link.h"

#include "ergo/common/json_min.h"
#include "ergo/common/ws_client.h"
#include "ergo/particle/particle_system.h"

#include <mutex>
#include <utility>

namespace ergo::particle {

namespace {

namespace jsonm = ::ergo::common::jsonm;
namespace ws    = ::ergo::common::ws;

/// decode_state_frame() that also reports the canonical text of `config`
/// (jsonm objects serialize with sorted keys), used to skip re-sent configs.
bool decode_state(const std::string& frame, ParticleEffectConfig& out, std::string& config_text) {
    jsonm::JsonValue root;
    if (!jsonm::parse(frame, root) || !root.is_object()) return false;
    const jsonm::JsonValue* op = root.find("op");
    if (!op || op->as_string() != "state") return false;
    const jsonm::JsonValue* config = root.find("config");
    if (!config || !config->is_object()) return false;

    // Hand only `config` to the particle parser: given the whole frame it
    // would skip the unknown top-level keys, succeed, and apply nothing.
    std::string text = jsonm::serialize(*config);
    ParticleEffectConfig cfg;
    if (!parse_config_json(text, cfg)) return false;
    out         = std::move(cfg);
    config_text = std::move(text);
    return true;
}

} // namespace

bool decode_state_frame(const std::string& frame, ParticleEffectConfig& out) {
    std::string unused;
    return decode_state(frame, out, unused);
}

struct EditorLink::Impl {
    ws::Client           client;
    bool                 started = false;

    std::mutex           mtx;          // guards everything below
    ParticleEffectConfig latest;
    std::string          latest_text;  // canonical `config` JSON of `latest`
    uint64_t             generation = 0;
    uint64_t             seen       = 0;

    // Socket worker thread (or the caller of feed()).
    bool on_text(const std::string& frame) {
        ParticleEffectConfig cfg;
        std::string text;
        if (!decode_state(frame, cfg, text)) return false;
        std::lock_guard<std::mutex> lk(mtx);
        // The plugin re-broadcasts `state` whenever a client joins or leaves;
        // only a different config is news for the host.
        if (generation != 0 && text == latest_text) return false;
        latest      = std::move(cfg);
        latest_text = std::move(text);
        ++generation;
        return true;
    }
};

EditorLink::EditorLink() : impl_(std::make_unique<Impl>()) {
    impl_->client.set_on_text([impl = impl_.get()](const std::string& t) { impl->on_text(t); });
}

bool EditorLink::feed(const std::string& frame) { return impl_->on_text(frame); }

EditorLink::~EditorLink() { disconnect(); }

bool EditorLink::connect(const std::string& host, uint16_t port, const std::string& path) {
    if (impl_->started || host.empty() || port == 0) return false;
    impl_->started = true;
    impl_->client.start(host, port, path.empty() ? std::string("/particle/ws") : path);
    return true;
}

void EditorLink::disconnect() {
    if (!impl_->started) return;
    impl_->client.stop();
    impl_->started = false;
}

bool EditorLink::is_connected() const { return impl_->client.is_connected(); }

bool EditorLink::poll(ParticleEffectConfig& out) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (impl_->generation == impl_->seen) return false;
    impl_->seen = impl_->generation;
    out = impl_->latest;
    return true;
}

bool EditorLink::apply_to(ParticleSystem& system) {
    ParticleEffectConfig cfg;
    if (!poll(cfg)) return false;
    system.set_config(cfg);
    return true;
}

} // namespace ergo::particle
