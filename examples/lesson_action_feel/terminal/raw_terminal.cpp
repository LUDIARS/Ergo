// The terminal backend of the playable front-end, one translation unit for every platform.
// POSIX (Linux / macOS): termios raw mode, non-blocking reads, restore on close / exit / signal.
// Windows: the playable terminal front-end is not implemented (a console raw-input backend would
// need ReadConsoleInput and virtual-terminal output, and could not be run where this sample was
// written). open() fails loudly so `--play` stops with this message instead of doing nothing; the
// headless modes (--check, the Studio link) do not use this file.
#include "raw_terminal.h"

#if defined(_WIN32)

namespace lesson_action_feel::terminal {

RawTerminal::~RawTerminal() = default;

std::string RawTerminal::open() {
    return "--play is not available on Windows in this build (the terminal front-end is POSIX only; "
           "use the headless mode or WSL)";
}

void RawTerminal::close() {}

std::string RawTerminal::read_available() { return {}; }

bool RawTerminal::write_all(std::string_view) { return false; }

bool RawTerminal::stop_requested() { return false; }

} // namespace lesson_action_feel::terminal

#else // POSIX

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace lesson_action_feel::terminal {

namespace {

termios           g_saved{};
std::atomic<bool> g_have_saved{false};
std::atomic<int>  g_stop{0};
bool              g_atexit_registered = false;

/// Async-signal-safe: tcsetattr and write are on the POSIX list.
void restore_saved() {
    if (!g_have_saved.exchange(false)) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_saved);
    static const char show_cursor[] = "\x1b[?25h";
    ssize_t           ignored       = write(STDOUT_FILENO, show_cursor, sizeof(show_cursor) - 1);
    (void)ignored;   // nothing more can be done from here if the terminal is gone
}

void on_stop_signal(int) {
    g_stop = 1;
    restore_saved();   // the loop also stops and restores; this covers a loop that cannot get there
}

} // namespace

RawTerminal::~RawTerminal() { close(); }

std::string RawTerminal::open() {
    if (open_) return {};
    if (isatty(STDIN_FILENO) == 0 || isatty(STDOUT_FILENO) == 0)
        return "--play needs a terminal on stdin and stdout (run it in a terminal, not through a pipe)";
    termios t{};
    if (tcgetattr(STDIN_FILENO, &t) != 0) return std::string("tcgetattr failed: ") + std::strerror(errno);
    g_saved = t;
    t.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | ISIG | IEXTEN));
    t.c_iflag &= static_cast<tcflag_t>(~(IXON | ICRNL));
    t.c_oflag &= static_cast<tcflag_t>(~OPOST);
    t.c_cc[VMIN]  = 0;   // read() returns at once with whatever is there
    t.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &t) != 0) return std::string("tcsetattr failed: ") + std::strerror(errno);
    g_have_saved = true;
    if (!g_atexit_registered) {
        std::atexit(restore_saved);
        g_atexit_registered = true;
    }
    g_stop = 0;
    std::signal(SIGTERM, on_stop_signal);
    std::signal(SIGHUP, on_stop_signal);
    std::signal(SIGINT, on_stop_signal);   // ISIG is off, but `kill -INT` still arrives
    open_ = true;
    return {};
}

void RawTerminal::close() {
    if (!open_) return;
    restore_saved();
    open_ = false;
}

std::string RawTerminal::read_available() {
    std::string out;
    char        buf[256];
    for (;;) {
        const ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n > 0) {
            out.append(buf, static_cast<std::size_t>(n));
            if (static_cast<std::size_t>(n) < sizeof(buf)) break;
            continue;
        }
        break;   // 0 = nothing typed (VMIN 0); < 0 = EINTR / EAGAIN, try again next frame
    }
    return out;
}

bool RawTerminal::write_all(std::string_view bytes) {
    while (!bytes.empty()) {
        const ssize_t n = write(STDOUT_FILENO, bytes.data(), bytes.size());
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            return false;
        }
        bytes.remove_prefix(static_cast<std::size_t>(n));
    }
    return true;
}

bool RawTerminal::stop_requested() { return g_stop != 0; }

} // namespace lesson_action_feel::terminal

#endif
