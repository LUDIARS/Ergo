#pragma once
/// A child process that keeps running (a game build, the game, ffmpeg): its
/// stdout / stderr arrive line by line while it runs, and terminate() stops it
/// together with everything it started (a Windows job object). Program lookup
/// and argument quoting follow process_runner.h: nothing goes through a shell
/// except a .cmd / .bat shim, whose arguments must be cmd_safe_arg().
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ergo::studio::platform {

struct LiveProcessSpec {
    std::string              program;   ///< name looked up on PATH, or a path
    std::vector<std::string> args;
    std::filesystem::path    cwd;       ///< empty = inherit
};

struct LiveProcessHandlers {
    /// One output line without its line break (`is_stderr` tells the stream).
    /// Called on a reader thread; lines longer than 4 KiB are cut.
    std::function<void(bool is_stderr, std::string_view line)> on_line;
    /// Once, after both streams ended and the process exited (-1 when terminated).
    /// Called on the waiter thread: it must not destroy the ILiveProcess.
    std::function<void(int exit_code)> on_exit;
};

class ILiveProcess {
public:
    virtual ~ILiveProcess() = default;   ///< terminates and waits for the reader threads
    /// Stop the process and its children. Safe from any thread, more than once.
    virtual void     terminate()     = 0;
    virtual bool     running() const = 0;
    virtual uint32_t pid() const     = 0;
};

/// nullptr (with `err`) when the program is missing or cannot start.
std::unique_ptr<ILiveProcess> start_live_process(const LiveProcessSpec& spec, LiveProcessHandlers handlers,
                                                 std::string& err);

/// Splits a byte stream into lines ('\n', a trailing '\r' dropped), cutting
/// each at `max_line` bytes. Pure; the readers use it.
class LineSplitter {
public:
    explicit LineSplitter(std::size_t max_line = 4096) : max_line_(max_line) {}
    void push(std::string_view chunk, const std::function<void(std::string_view)>& emit);
    void finish(const std::function<void(std::string_view)>& emit);

private:
    std::size_t max_line_;
    std::string pending_;
    bool        cutting_ = false;   ///< the rest of an over-long line is skipped
};

} // namespace ergo::studio::platform
