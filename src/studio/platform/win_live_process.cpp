// Windows ILiveProcess: CreateProcessW in a kill-on-close job object, stdout /
// stderr read on their own threads, a waiter that reports the exit after both.
#include "ergo/studio/platform/live_process.h"
#include "win_command.h"

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <mutex>
#include <thread>

namespace ergo::studio::platform {
namespace {

class WinLiveProcess final : public ILiveProcess {
public:
    WinLiveProcess(HANDLE process, HANDLE job, DWORD pid, HANDLE out_r, HANDLE err_r, LiveProcessHandlers handlers)
        : process_(process), job_(job), pid_(pid), handlers_(std::move(handlers)) {
        out_thread_ = std::thread([this, out_r] { read(out_r, false); });
        err_thread_ = std::thread([this, err_r] { read(err_r, true); });
        wait_thread_ = std::thread([this] { wait(); });
    }

    ~WinLiveProcess() override {
        terminate();
        // The waiter joins the readers; joining it is enough.
        if (wait_thread_.joinable()) wait_thread_.join();
        CloseHandle(process_);
        CloseHandle(job_);
    }

    void terminate() override {
        if (!running_.load()) return;
        terminated_.store(true);
        // Closing the job would also work, but the handle is still needed by the waiter.
        TerminateJobObject(job_, 1);
    }

    bool     running() const override { return running_.load(); }
    uint32_t pid() const override { return pid_; }

private:
    void read(HANDLE pipe, bool is_stderr) {
        LineSplitter splitter;
        const auto   emit = [this, is_stderr](std::string_view line) {
            if (handlers_.on_line) handlers_.on_line(is_stderr, line);
        };
        char  buf[8192];
        DWORD got = 0;
        while (ReadFile(pipe, buf, sizeof(buf), &got, nullptr) && got > 0) splitter.push(std::string_view(buf, got), emit);
        splitter.finish(emit);
        CloseHandle(pipe);
    }

    void wait() {
        WaitForSingleObject(process_, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(process_, &code);
        // The children may still hold the pipes: end them with the parent.
        TerminateJobObject(job_, 1);
        if (out_thread_.joinable()) out_thread_.join();
        if (err_thread_.joinable()) err_thread_.join();
        running_.store(false);
        if (handlers_.on_exit) handlers_.on_exit(terminated_.load() ? -1 : static_cast<int>(code));
    }

    HANDLE              process_;
    HANDLE              job_;
    DWORD               pid_;
    LiveProcessHandlers handlers_;
    std::atomic<bool>   running_{true};
    std::atomic<bool>   terminated_{false};
    std::thread         out_thread_, err_thread_, wait_thread_;
};

} // namespace

std::unique_ptr<ILiveProcess> start_live_process(const LiveProcessSpec& spec, LiveProcessHandlers handlers,
                                                 std::string& err) {
    WinCommand command;
    if (!build_win_command(spec.program, spec.args, command, err)) return nullptr;

    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job == nullptr) {
        err = "CreateJobObject failed (error " + std::to_string(GetLastError()) + ")";
        return nullptr;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));

    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE in_r = nullptr, in_w = nullptr, out_r = nullptr, out_w = nullptr, err_r = nullptr, err_w = nullptr;
    if (!CreatePipe(&in_r, &in_w, &sa, 0) || !CreatePipe(&out_r, &out_w, &sa, 0) || !CreatePipe(&err_r, &err_w, &sa, 0)) {
        err = "CreatePipe failed";
        for (HANDLE h : {in_r, in_w, out_r, out_w, err_r, err_w}) {
            if (h != nullptr) CloseHandle(h);
        }
        CloseHandle(job);
        return nullptr;
    }
    SetHandleInformation(in_w, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdInput  = in_r;
    si.hStdOutput = out_w;
    si.hStdError  = err_w;
    PROCESS_INFORMATION pi{};
    std::wstring       wcmd = widen_utf8(command.cmdline);
    const std::wstring wcwd = spec.cwd.empty() ? std::wstring() : spec.cwd.wstring();
    // Suspended until it is in the job, so nothing it starts escapes the job.
    const BOOL ok = CreateProcessW(command.app.c_str(), wcmd.data(), nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, wcwd.empty() ? nullptr : wcwd.c_str(),
                                   &si, &pi);
    CloseHandle(in_r);
    CloseHandle(out_w);
    CloseHandle(err_w);
    CloseHandle(in_w);   // the children read no input
    if (!ok) {
        err = "CreateProcess failed (error " + std::to_string(GetLastError()) + ")";
        CloseHandle(out_r);
        CloseHandle(err_r);
        CloseHandle(job);
        return nullptr;
    }
    if (!AssignProcessToJobObject(job, pi.hProcess)) {
        err = "AssignProcessToJobObject failed (error " + std::to_string(GetLastError()) + ")";
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        CloseHandle(out_r);
        CloseHandle(err_r);
        CloseHandle(job);
        return nullptr;
    }
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    return std::make_unique<WinLiveProcess>(pi.hProcess, job, pi.dwProcessId, out_r, err_r, std::move(handlers));
}

} // namespace ergo::studio::platform
