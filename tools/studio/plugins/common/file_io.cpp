#include "file_io.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>
#include <system_error>

namespace ergo::studio::plugins {
namespace fs = std::filesystem;

std::optional<std::string> read_text_file(const fs::path& p, std::string& err) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        err = "cannot open " + p.u8string();
        return std::nullopt;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    if (in.bad()) {
        err = "read failed: " + p.u8string();
        return std::nullopt;
    }
    return ss.str();
}

bool write_text_file(const fs::path& p, const std::string& content, std::string& err) {
    std::error_code ec;
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    if (ec) {
        err = "cannot create " + p.parent_path().u8string() + ": " + ec.message();
        return false;
    }
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) {
        err = "cannot open " + p.u8string() + " for writing";
        return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!out) {
        err = "write failed: " + p.u8string();
        return false;
    }
    return true;
}

std::string iso_utc(fs::file_time_type t) {
    // C++17 has no clock_cast: shift by the distance between the two clocks' "now".
    using namespace std::chrono;
    const auto sys = time_point_cast<milliseconds>(t - fs::file_time_type::clock::now() + system_clock::now());
    const std::time_t secs = system_clock::to_time_t(time_point_cast<system_clock::duration>(sys));
    const long long   ms   = (sys.time_since_epoch().count() % 1000 + 1000) % 1000;
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &secs);
#else
    gmtime_r(&secs, &tm);
#endif
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03lldZ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec, ms);
    return buf;
}

} // namespace ergo::studio::plugins
