#pragma once

/// ergo::io::path — thin string-path helpers built on std::filesystem.
///
/// Public API uses `std::string` (UTF-8) so hosts don't have to leak
/// `std::filesystem::path` into their interfaces.

#include <string>

namespace ergo::io::path {

/// Parent directory ("foo/bar/baz.txt" -> "foo/bar").
std::string parent_of(const std::string& path);

/// Cross-platform join. Accepts either separator in inputs; output uses
/// the OS preferred separator.
std::string join(const std::string& a, const std::string& b);

/// Extension including the dot (".txt", or "" if none).
std::string extension_of(const std::string& path);

/// Filename without directory or extension ("foo/bar/baz.txt" -> "baz").
std::string stem_of(const std::string& path);

} // namespace ergo::io::path
