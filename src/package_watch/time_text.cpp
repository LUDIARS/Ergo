#include "ergo/package_watch/time_text.h"

#include <cstdio>

namespace ergo::package_watch {

namespace {

// 1970-01-01 からの日数と暦日の変換 (proleptic Gregorian、 H. Hinnant の civil_from_days / days_from_civil)。
void civil_from_days(std::int64_t z, int& y, int& m, int& d) {
    z += 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const std::int64_t doe = z - era * 146097;
    const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const std::int64_t mp  = (5 * doy + 2) / 153;
    d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    y = static_cast<int>(yoe + era * 400 + (m <= 2 ? 1 : 0));
}

std::int64_t days_from_civil(int y, int m, int d) {
    y -= m <= 2 ? 1 : 0;
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const std::int64_t yoe = y - era * 400;
    const std::int64_t doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const std::int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

bool digits(const std::string& s, std::size_t at, std::size_t n, int& out) {
    out = 0;
    for (std::size_t i = at; i < at + n; ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
        out = out * 10 + (s[i] - '0');
    }
    return true;
}

} // namespace

std::string format_utc(std::int64_t seconds) {
    std::int64_t days = seconds / 86400;
    std::int64_t rest = seconds % 86400;
    if (rest < 0) {
        rest += 86400;
        --days;
    }
    int y = 0, m = 0, d = 0;
    civil_from_days(days, y, m, d);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ", y, m, d,
                  static_cast<int>(rest / 3600), static_cast<int>(rest / 60 % 60),
                  static_cast<int>(rest % 60));
    return buf;
}

bool parse_utc(const std::string& text, std::int64_t& out) {
    if (text.size() != 20 || text[4] != '-' || text[7] != '-' || text[10] != 'T' || text[13] != ':' ||
        text[16] != ':' || text[19] != 'Z') {
        return false;
    }
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
    if (!digits(text, 0, 4, y) || !digits(text, 5, 2, mo) || !digits(text, 8, 2, d) ||
        !digits(text, 11, 2, h) || !digits(text, 14, 2, mi) || !digits(text, 17, 2, s)) {
        return false;
    }
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 59) return false;
    out = days_from_civil(y, mo, d) * 86400 + h * 3600 + mi * 60 + s;
    return true;
}

} // namespace ergo::package_watch
