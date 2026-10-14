#pragma once

// UTC 秒と ISO 8601 ("2026-10-15T09:30:00Z") の相互変換。 ロケールと時刻帯に依らない (出力を決定的にする)。

#include <cstdint>
#include <string>

namespace ergo::package_watch {

std::string format_utc(std::int64_t seconds);

/// "YYYY-MM-DDTHH:MM:SSZ" だけを読む。 違えば false。
bool parse_utc(const std::string& text, std::int64_t& out);

} // namespace ergo::package_watch
