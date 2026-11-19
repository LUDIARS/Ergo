#pragma once
/// SonusLink: the Studio's side of Sonus' live link — one snd::tool::Session
/// (Sonus::tool_core: connection, mixer with undo / redo, overrides, profiler,
/// recording) behind a C++17 interface. sonus_link.cpp is the only file that
/// includes Sonus headers and is compiled as C++20 in its own target
/// (ergo_studio_sonus_link); everything that includes this header stays C++17.
///
/// Not thread-safe, like the Session: one thread at a time (the plugin holds
/// its mutex around every call). Polling only: tick() pumps the session; the
/// network runs on the live link client's own thread.
#include "sonus_shown.h"

#include <cstdint>
#include <memory>
#include <string>

namespace ergo::studio::plugins::sonus {

class SonusLink {
public:
    SonusLink();
    ~SonusLink();
    SonusLink(const SonusLink&)            = delete;
    SonusLink& operator=(const SonusLink&) = delete;

    /// Reads the project (and builds it into `build_dir` when not empty, for its
    /// build hash). false when it has errors (see shown().project_errors); the
    /// mixer then shows what could be read.
    bool load_project(const std::string& dir, const std::string& build_dir);

    /// Opens the live link (the client keeps reconnecting until disconnect()).
    /// "" on success, else the reason (snd_result name).
    std::string connect(const std::string& host, uint16_t port);
    void        disconnect();
    bool        is_open() const;
    void        tick();

    // ---- mixer (undoable). false when the bus does not exist or nothing changes.
    bool set_bus_fader(int bus, float db);
    bool set_bus_mute(int bus, bool on);
    bool set_bus_solo(int bus, bool on);
    bool undo();
    bool redo();
    /// The bus with this key, -1 if none.
    int  find_bus(const std::string& key) const;

    // ---- overrides
    /// false when no override has this key.
    bool revert(const std::string& key);
    bool revert_all();

    // ---- recording
    bool start_recording();
    bool stop_recording();
    /// "" on success, else why (not ready, file error).
    std::string save_recording(const std::string& path);

    SonusShown shown() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ergo::studio::plugins::sonus
