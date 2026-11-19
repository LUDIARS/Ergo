// POSIX の launch: posix_spawnp に argv をそのまま渡す (/bin/sh を挟まない)。
#ifndef _WIN32
#include "process_launch.h"

#include <cerrno>
#include <cstdio>

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace ergo::package_cli {

namespace {

/// posix_spawn_file_actions_t を確実に破棄する。
class FileActions {
public:
    FileActions() { ok_ = posix_spawn_file_actions_init(&actions_) == 0; }
    FileActions(const FileActions&) = delete;
    FileActions& operator=(const FileActions&) = delete;
    ~FileActions() {
        if (ok_) posix_spawn_file_actions_destroy(&actions_);
    }
    bool ok() const { return ok_; }
    posix_spawn_file_actions_t* get() { return &actions_; }

    /// fd を mode に合わせてつなぐ (capture のときはパイプの書き端 write_end を dup2 する)。
    bool wire(int fd, StreamMode mode, int write_end) {
        switch (mode) {
            case StreamMode::inherit: return true;
            case StreamMode::discard:
                return posix_spawn_file_actions_addopen(&actions_, fd, "/dev/null", O_WRONLY, 0) == 0;
            case StreamMode::capture: return posix_spawn_file_actions_adddup2(&actions_, write_end, fd) == 0;
        }
        return false;
    }

private:
    posix_spawn_file_actions_t actions_{};
    bool ok_ = false;
};

class CapturePipe {
public:
    CapturePipe() = default;
    CapturePipe(const CapturePipe&) = delete;
    CapturePipe& operator=(const CapturePipe&) = delete;
    ~CapturePipe() { close_read(); close_write(); }

    bool open() {
        int ends[2];
#if defined(__linux__)
        if (pipe2(ends, O_CLOEXEC) != 0) return false;
#else
        if (pipe(ends) != 0) return false;
#endif
        // Keep pipe descriptors above stderr even if the caller closed a standard descriptor.
        read_ = fcntl(ends[0], F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
        write_ = fcntl(ends[1], F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
        close(ends[0]);
        close(ends[1]);
        return read_ >= 0 && write_ >= 0;
    }

    int writer() const { return write_; }
    void close_write() { close_end(write_); }

    bool drain(std::string& output) {
        char buffer[4096];
        for (;;) {
            const ssize_t count = read(read_, buffer, sizeof(buffer));
            if (count == 0) return true;
            if (count > 0) output.append(buffer, static_cast<std::size_t>(count));
            else if (errno != EINTR) return false;
        }
    }

    void close_read() { close_end(read_); }

private:
    static void close_end(int& fd) {
        if (fd >= 0) close(fd);
        fd = -1;
    }

    int read_ = -1;
    int write_ = -1;
};

} // namespace

int launch(const std::vector<std::string>& argv, StreamMode out_mode, StreamMode err_mode, std::string* captured) {
    if (argv.empty() || err_mode == StreamMode::capture) return -1;
    if (out_mode == StreamMode::capture && captured == nullptr) return -1;

    std::vector<char*> args;
    args.reserve(argv.size() + 1);
    for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
    args.push_back(nullptr);

    CapturePipe pipe;
    const bool capture_stdout = out_mode == StreamMode::capture;
    if (capture_stdout && !pipe.open()) return -1;
    FileActions actions;
    if (!actions.ok() || !actions.wire(STDOUT_FILENO, out_mode, pipe.writer()) ||
        !actions.wire(STDERR_FILENO, err_mode, -1)) return -1;

    std::fflush(stdout);
    std::fflush(stderr);
    pid_t pid = -1;
    const int spawned = posix_spawnp(&pid, args.front(), actions.get(), nullptr, args.data(), environ);
    pipe.close_write();   // The parent must release its writer for the reader to see EOF.
    if (spawned != 0) return -1;
    const bool read_ok = !capture_stdout || pipe.drain(*captured);
    pipe.close_read();

    int wait_status = 0;
    while (waitpid(pid, &wait_status, 0) < 0) {
        if (errno != EINTR) return -1;
    }
    return read_ok && WIFEXITED(wait_status) ? WEXITSTATUS(wait_status) : -1;
}

} // namespace ergo::package_cli
#endif
