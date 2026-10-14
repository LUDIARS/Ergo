#include "status_inputs.h"

#include "file_io.h"

#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)
#include <filesystem>
#endif

namespace ergo::package_cli {

namespace {

/// 更新時刻 (time_t)。 取れなければ false。 Windows は UTF-8 のパスを広い文字で開く。
bool mtime_of(const std::string& path, std::time_t& out) {
#if defined(_WIN32)
    struct _stat64 st;
    if (_wstat64(std::filesystem::u8path(path).c_str(), &st) != 0) return false;
    out = static_cast<std::time_t>(st.st_mtime);
#else
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) return false;
    out = st.st_mtime;
#endif
    return true;
}

} // namespace

std::string file_mtime_utc(const std::string& path) {
    std::time_t t = 0;
    if (!mtime_of(path, t)) return {};
    std::tm tm{};
#if defined(_WIN32)
    if (gmtime_s(&tm, &t) != 0) return {};
#else
    if (!gmtime_r(&t, &tm)) return {};
#endif
    char buf[32];
    if (std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm) == 0) return {};
    return buf;
}

bool collect_test_tally(const std::string& build, const std::string& junit,
                        package_manager::TestTally& out, std::string* error) {
    out = package_manager::TestTally{};
    if (!junit.empty()) {
        const auto xml = read_file(junit);
        if (!xml) {
            if (error) *error = "cannot read --junit " + junit;
            return false;
        }
        if (!package_manager::parse_junit(*xml, out)) {
            if (error) *error = "--junit " + junit + " has no <testcase>";
            return false;
        }
        return true;
    }
    if (const auto log = read_file(join(build, "Testing/Temporary/LastTest.log"))) {
        if (!package_manager::parse_last_test_log(*log, out)) out = package_manager::TestTally{};
    }
    return true;
}

} // namespace ergo::package_cli
