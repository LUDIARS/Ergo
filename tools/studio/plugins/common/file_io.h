#pragma once
/// Whole-file text I/O for plugins (bytes in, bytes out; no newline translation).
#include <filesystem>
#include <optional>
#include <string>

namespace ergo::studio::plugins {

/// File contents, or nullopt with `err` set.
std::optional<std::string> read_text_file(const std::filesystem::path& p, std::string& err);

/// Write `content`, creating missing parent directories. False with `err` set.
bool write_text_file(const std::filesystem::path& p, const std::string& content, std::string& err);

/// UTC ISO-8601 with milliseconds ("2026-11-06T01:02:03.000Z") of a file time.
std::string iso_utc(std::filesystem::file_time_type t);

} // namespace ergo::studio::plugins
