#pragma once
/// "sonus" plugin: the Studio end of Sonus' live link (spec/feature/tool/sonus.md).
/// Connects to the running game's Sonus (ERGO_SONUS_HOST:ERGO_SONUS_PORT) and
/// offers the mixer (fader / mute / solo, undo / redo), the overrides (revert /
/// revert all), the profiler (meters, voices, timings, memory, log) and the
/// recording (start / stop / save) — in the tab (tools/studio/views/sonus_view)
/// and as /sonus/api/* routes (sonus_routes.h).
///
/// Threads: one ticker thread polls the SonusLink every tick_ms and publishes a
/// SonusShown; the UI thread and the routes call the in-process API below. A
/// mutex serialises every SonusLink call (the Sonus session is one thread at a
/// time). Built only when Sonus is in the build (CMake: Sonus + ERGO_BUILD_STUDIO).
#include "sonus_config.h"
#include "sonus_link.h"

#include "ergo/studio/host/plugin.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace ergo::studio::plugins::sonus {

/// What an edit did.
enum class Edit : uint8_t {
    Changed   = 0,
    Unchanged = 1,   ///< same value, nothing to undo / redo / revert
    NotFound  = 2,   ///< no bus / override with that key
};

class SonusPlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginHealth {
public:
    SonusPlugin(SonusConfig config, host::LogFn log);
    ~SonusPlugin() override;   ///< stops and joins the ticker, closes the link

    SonusPlugin(const SonusPlugin&)            = delete;
    SonusPlugin& operator=(const SonusPlugin&) = delete;

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginHealth*    health() override { return this; }
    host::IPluginView*      view() override { return view_.get(); }
    /// {ok (project usable), state, target, project}.
    host::JsonValue   snapshot() const override;
    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;

    /// The tab. Set once at the composition root, before the window runs.
    void attach_view(std::shared_ptr<host::IPluginView> view) { view_ = std::move(view); }

    // ---- In-process API (view, routes, tests) ------------------------------------------
    /// Opens the link to config().host:port. "" on success, else why (already open, Sonus error).
    std::string connect();
    void        disconnect();

    Edit set_bus_fader(const std::string& bus_key, float db);
    Edit nudge_bus_fader(const std::string& bus_key, float delta_db);   ///< from the current value; -inf counts as kFloorDb
    Edit set_bus_mute(const std::string& bus_key, bool on);
    Edit set_bus_solo(const std::string& bus_key, bool on);
    Edit undo();
    Edit redo();
    Edit revert(const std::string& override_key);
    Edit revert_all();

    /// false when there is no connected game or a recording is in progress.
    bool start_recording();
    bool stop_recording();
    /// Saves the finished recording under config().recording_dir. Returns the path,
    /// or "" with `error` set (saving not configured, not ready, file error).
    std::string save_recording(std::string& error);
    std::string last_saved() const;

    SonusShown         shown() const;
    uint64_t           revision() const { return revision_.load(); }
    const SonusConfig& config() const { return config_; }

private:
    void run() noexcept;
    void loop();
    void publish();                        ///< mtx_ held: shown_ from the link, logs changes
    Edit bus_edit(const std::string& key, bool (SonusLink::*fn)(int, bool), bool on);

    host::PluginInfo                   info_;
    SonusConfig                        config_;
    host::LogFn                        log_;
    std::shared_ptr<host::IPluginView> view_;

    mutable std::mutex      mtx_;
    std::condition_variable cv_;
    bool                    stop_ = false;
    SonusLink               link_;
    bool                    project_ok_ = true;
    SonusShown              shown_;
    std::string             last_saved_;
    unsigned                saves_ = 0;
    std::atomic<uint64_t>   revision_{0};

    std::thread ticker_;   ///< last member: started when everything above exists
};

std::unique_ptr<SonusPlugin> make_sonus_plugin(const SonusConfig& config, const host::LogFn& log);

} // namespace ergo::studio::plugins::sonus
