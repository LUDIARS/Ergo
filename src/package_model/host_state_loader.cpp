// ergo_package_model: P4 の入力 3 つをまとめて読む。 欠けても壊れていても失敗せず、 理由を notes に残す。

#include "ergo/package_model/host_state_reader.h"

#include "ergo/package_model/resolved_reader.h"
#include "json_access.h"

#include <filesystem>

namespace ergo::package_model {

namespace {

bool exists(const std::string& path) {
    std::error_code ec;
    return !path.empty() && std::filesystem::is_regular_file(std::filesystem::u8path(path), ec);
}

/// 読む: 無ければ「無し」、 読めない・壊れていれば理由。 どちらも notes に 1 行。
template <typename Reader>
bool load_one(const std::string& path, const char* what, Reader&& reader, HostState& state) {
    if (path.empty()) {
        state.notes.push_back(std::string(what) + ": not given");
        return false;
    }
    if (!exists(path)) {
        state.notes.push_back(std::string(what) + ": not found");
        return false;
    }
    std::string text;
    if (!detail::read_text_file(path, text)) {
        state.notes.push_back(std::string(what) + ": cannot read");
        return false;
    }
    std::string error;
    if (!reader(text, &error)) {
        state.notes.push_back(error);
        return false;
    }
    return true;
}

} // namespace

HostState load_host_state(const HostStatePaths& paths) {
    HostState s;
    s.has_resolved = load_one(paths.resolved, "ergo-packages.resolved.json",
        [&](const std::string& t, std::string* e) { return read_resolved_text(t, s.resolved, e); }, s);
    s.has_report = load_one(paths.report, "watch-report.json",
        [&](const std::string& t, std::string* e) { return read_watch_report_text(t, s.report, e); }, s);
    s.has_status = load_one(paths.status, "ergo-packages.status.json",
        [&](const std::string& t, std::string* e) { return read_host_status_text(t, s.status, e); }, s);
    return s;
}

} // namespace ergo::package_model
