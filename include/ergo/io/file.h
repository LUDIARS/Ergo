#pragma once

/// ergo::io — minimal cross-platform file I/O wrapper.
///
/// Thin, exception-free surface over `<filesystem>` + `<fstream>`. All
/// paths are UTF-8 `std::string`. Every function returns `bool` so
/// callers write straightforward `if (!read_file(...)) fail()` code.
///
/// Not in scope: JSON / binary protocol encoding, mmap, async I/O, file
/// locking. Modules that need those build on top of this.

#include <cstdint>
#include <string>
#include <vector>

namespace ergo::io {

// -----------------------------------------------------------------------
// Read
// -----------------------------------------------------------------------

/// Read the entire file into `out`. Returns false if the file is
/// missing or unreadable; `out` is left cleared on failure.
bool read_file(const std::string& path, std::string& out);

/// Binary variant — bytes straight into a `vector<uint8_t>`.
bool read_file_bytes(const std::string& path, std::vector<uint8_t>& out);

// -----------------------------------------------------------------------
// Write
// -----------------------------------------------------------------------

/// Write `content` to `path`, truncating any existing file. Creates
/// parent directories as needed. Returns true on success.
bool write_file(const std::string& path, const std::string& content);

/// Binary variant.
bool write_file_bytes(const std::string& path,
                      const std::vector<uint8_t>& bytes);

// -----------------------------------------------------------------------
// Queries
// -----------------------------------------------------------------------

bool exists       (const std::string& path);
bool is_directory (const std::string& path);

/// Create `path` and all missing parents. Returns true if the directory
/// exists on completion (created or pre-existing).
bool ensure_directory(const std::string& path);

/// Delete a single file (not directories). Returns true on success
/// *or* when the file didn't exist to begin with.
bool remove_file(const std::string& path);

} // namespace ergo::io
