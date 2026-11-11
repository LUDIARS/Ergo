#pragma once
/// "lesson" plugin: links this Studio to a class of odeum-lesson
/// (spec/feature/tool/lesson.md). As a student it reports assignment steps
/// whose check the running game satisfies; as the presenter it switches demo
/// features in the running game when the lesson reaches a demo cue.
///
/// One worker thread owns the LessonSession and does all HTTP; the UI thread
/// (and the plugin's own routes, lesson_routes.h) only queue commands and read
/// the last published LessonShown. The token never leaves the session.
#include "lesson_session.h"

#include "ergo/studio/host/plugin.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace ergo::studio::plugins::lesson {

/// What the tab shows. No token, no link code.
struct LessonShown {
    bool        connecting = false;   ///< a pair request is queued or running
    std::string link_error;           ///< why the pasted connection string was rejected, or why the worker stopped
    SessionView session;
};

class LessonPlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginHealth {
public:
    LessonPlugin(std::shared_ptr<platform::IHttpClient> http, std::shared_ptr<IGameVariables> variables, host::LogFn log);
    ~LessonPlugin() override;   ///< stops and joins the worker (waits for a call in flight, at most its timeout)

    LessonPlugin(const LessonPlugin&)            = delete;
    LessonPlugin& operator=(const LessonPlugin&) = delete;

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginHealth*    health() override { return this; }
    host::IPluginView*      view() override { return view_.get(); }
    /// {ok, linked, role} only.
    JsonValue snapshot() const override;
    /// /lesson/api/state, /lesson/api/connect, /lesson/api/disconnect (lesson_routes.h).
    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;

    /// The tab (tools/studio/views). Set once at the composition root, before the window runs.
    void attach_view(std::shared_ptr<host::IPluginView> view) { view_ = std::move(view); }

    // ---- In-process API (the view and the routes; never blocks on HTTP) ----
    /// Validate the pasted connection string and queue the pairing. A rejected string is reported in shown()
    /// and returned (the reason never contains the input); nullopt when the pairing was queued.
    std::optional<std::string> connect(const std::string& link_text);
    void        disconnect();
    LessonShown shown() const;
    /// Bumped whenever shown() changed.
    uint64_t    revision() const { return revision_.load(); }

private:
    void run() noexcept;
    void work();
    bool has_command() const { return stop_ || wants_disconnect_ || wanted_link_.has_value(); }   // mtx_ held
    void publish(bool connecting);          // mtx_ held; worker thread
    void log_changes(const SessionView& before, const SessionView& after) const;

    host::PluginInfo                   info_;
    host::LogFn                        log_;
    std::shared_ptr<host::IPluginView> view_;

    mutable std::mutex        mtx_;
    std::condition_variable   cv_;
    bool                      stop_ = false;
    bool                      wants_disconnect_ = false;
    std::optional<LessonLink> wanted_link_;
    LessonShown               shown_;
    std::atomic<uint64_t>     revision_{0};

    LessonSession session_;   ///< worker thread only
    std::thread   worker_;    ///< last member: started when everything above exists
};

std::unique_ptr<LessonPlugin> make_lesson_plugin(std::shared_ptr<IGameVariables> variables, const host::LogFn& log);

} // namespace ergo::studio::plugins::lesson
