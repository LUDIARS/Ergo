#include "recording_path.h"

#include <cstdio>

namespace ergo::studio::plugins::sonus {

std::string recording_file_name(std::time_t t, unsigned n) {
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &t);
#else
    gmtime_r(&t, &utc);
#endif
    char buf[64];
    std::snprintf(buf, sizeof buf, "sonus-%04d%02d%02d-%02d%02d%02d-%u.snrc", utc.tm_year + 1900, utc.tm_mon + 1,
                  utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec, n);
    return buf;
}

} // namespace ergo::studio::plugins::sonus
