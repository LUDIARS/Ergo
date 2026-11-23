// IRemotePlay over Odeum's odeum_remote_play (built when ERGO_ODEUM_SOURCE_DIR
// is set). Odeum captures, streams and injects input; this adapter maps the
// plugin's targets onto Odeum's, and fetches TURN credentials with the Studio's
// HTTP client (the key stays here, Odeum only sees the generated credentials).
#include "remote_play.h"

#include "ergo/studio/platform/http_client.h"

#include <odeum/remote_play/ice.hpp>
#include <odeum/remote_play/input.hpp>
#include <odeum/remote_play/stream_hub.hpp>

#include <chrono>
#include <mutex>

namespace ergo::studio::plugins::play {
namespace rp = odeum::remote_play;
namespace {

JsonValue to_json(const std::vector<rp::IceServer>& servers) {
    JsonValue out = JsonValue::make_array();
    for (const auto& s : servers) {
        JsonValue urls = JsonValue::make_array();
        for (const auto& u : s.urls) urls.push(jstr(u));
        JsonValue entry = jobj({{"urls", urls}});
        if (s.username) entry.set("username", jstr(*s.username));
        if (s.credential) entry.set("credential", jstr(*s.credential));
        out.push(entry);
    }
    return out;
}

rp::PointerPhase phase_of(PointerPhase p) {
    switch (p) {
        case PointerPhase::Down: return rp::PointerPhase::Down;
        case PointerPhase::Up: return rp::PointerPhase::Up;
        case PointerPhase::Move: break;
    }
    return rp::PointerPhase::Move;
}

class OdeumRemotePlay final : public IRemotePlay {
public:
    OdeumRemotePlay(const RemotePlayConfig& config, host::LogFn log)
        : stun_(rp::stun_servers(config.stun_urls)),
          key_id_(config.turn_key_id),
          key_token_(config.turn_key_token),
          http_(platform::make_system_http_client()),
          input_(rp::make_desktop_input()),
          hub_(rp::make_stream_hub({config.ffmpeg, std::move(log)})) {}

    bool streaming() const override { return hub_->available(); }
    bool input() const override { return input_->supported(); }
    bool turn() const override { return !key_id_.empty() && !key_token_.empty(); }

    JsonValue ice_servers(std::string* warning) override { return to_json(servers(warning)); }

    bool open(const std::string& viewer, const PlayTarget& t, const std::string& offer, std::function<void()> on_closed,
              std::string& answer, std::string& err) override {
        const rp::CaptureTarget target{t.name, t.window, t.fps, t.bitrate, t.root.u8string()};
        return hub_->open(viewer, target, offer, servers(nullptr), std::move(on_closed), answer, err);
    }
    void        close(const std::string& viewer) override { hub_->close(viewer); }
    std::size_t viewers(const std::string& target) const override { return hub_->viewers(target); }

    bool known_key(const std::string& name) const override { return rp::virtual_key_of(name).has_value(); }
    bool key(const std::string& window, const std::string& name, bool down, std::string& err) override {
        const auto vk = rp::virtual_key_of(name);
        if (!vk) {
            err = "unknown key";
            return false;
        }
        return input_->key(window, *vk, down, err);
    }
    bool pointer(const std::string& window, double x, double y, int button, PointerPhase phase, std::string& err) override {
        return input_->pointer(window, x, y, button, phase_of(phase), err);
    }
    bool wheel(const std::string& window, double x, double y, int notches, std::string& err) override {
        return input_->wheel(window, x, y, notches, err);
    }

private:
    /// STUN, plus TURN credentials reused until an hour before they expire.
    std::vector<rp::IceServer> servers(std::string* warning) {
        std::vector<rp::IceServer> out = stun_;
        if (!turn()) return out;
        std::lock_guard<std::mutex> lock(mutex_);
        const auto now = std::chrono::steady_clock::now();
        if (turn_servers_.empty() || now >= reuse_until_) {
            const auto [url, body] = rp::turn_request(key_id_);
            platform::HttpClientRequest req;
            req.method     = "POST";
            req.url        = url;
            req.body       = body;
            req.headers    = {{"Authorization", "Bearer " + key_token_}, {"Content-Type", "application/json"}};
            req.timeout_ms = 10000;
            const auto res   = http_->send(req);
            auto       fresh = res.ok() ? rp::parse_turn_response(res.body) : std::vector<rp::IceServer>{};
            if (fresh.empty()) {
                if (warning != nullptr) {
                    *warning = res.transport_ok ? "TURN credentials request answered HTTP " + std::to_string(res.status)
                                                : "TURN credentials request failed: " + res.error;
                }
            } else {
                turn_servers_ = std::move(fresh);
                reuse_until_  = now + std::chrono::seconds(rp::kTurnTtlSec - 3600);
            }
        }
        out.insert(out.end(), turn_servers_.begin(), turn_servers_.end());
        return out;
    }

    std::vector<rp::IceServer>             stun_;
    std::string                            key_id_, key_token_;
    std::shared_ptr<platform::IHttpClient> http_;
    std::shared_ptr<rp::DesktopInput>      input_;
    std::unique_ptr<rp::StreamHub>         hub_;
    std::mutex                             mutex_;
    std::vector<rp::IceServer>             turn_servers_;
    std::chrono::steady_clock::time_point  reuse_until_{};
};

} // namespace

std::unique_ptr<IRemotePlay> make_remote_play(const RemotePlayConfig& config, host::LogFn log) {
    return std::make_unique<OdeumRemotePlay>(config, std::move(log));
}

} // namespace ergo::studio::plugins::play
