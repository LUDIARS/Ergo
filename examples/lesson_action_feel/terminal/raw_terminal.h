#pragma once
/// lesson_action_feel terminal — the console the playable front-end needs:
/// raw, non-blocking keyboard input and plain byte output.
///
/// POSIX (Linux / macOS): termios raw mode (no echo, no line buffering, no
/// signal keys: Ctrl+C arrives as a byte), reads that never block. The saved
/// mode is restored by close(), by the destructor, at exit() and when SIGTERM /
/// SIGHUP / SIGINT arrive (they also ask the game loop to stop).
/// Windows: not implemented; open() fails with a message saying so (the
/// headless modes still work there). See the spec, "端末で遊ぶ".
#include <cstddef>
#include <string>
#include <string_view>

namespace lesson_action_feel::terminal {

class RawTerminal {
public:
    RawTerminal() = default;
    ~RawTerminal();
    RawTerminal(const RawTerminal&)            = delete;
    RawTerminal& operator=(const RawTerminal&) = delete;

    /// Take the terminal (stdin and stdout must be a terminal). "" on success, else why not.
    std::string open();
    /// Give it back (idempotent).
    void close();

    /// Bytes typed since the last call (may be empty; never blocks).
    std::string read_available();
    /// Write all of `bytes` to the terminal. false when the terminal is gone.
    bool write_all(std::string_view bytes);

    /// A stop signal (SIGTERM / SIGHUP / SIGINT) arrived since open().
    static bool stop_requested();

private:
    bool open_ = false;
};

} // namespace lesson_action_feel::terminal
