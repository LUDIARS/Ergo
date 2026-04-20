/// ergo_io dummy plug — every operation is a no-op / false. Host apps
/// that want to link the symbol surface without the real implementation
/// (e.g. tooling builds, static analysis) can use this.

#include "ergo/io/file.h"
#include "ergo/io/path.h"

namespace ergo::io {

bool read_file       (const std::string&, std::string&)                 { return false; }
bool read_file_bytes (const std::string&, std::vector<uint8_t>&)        { return false; }
bool write_file      (const std::string&, const std::string&)           { return false; }
bool write_file_bytes(const std::string&, const std::vector<uint8_t>&)  { return false; }
bool exists          (const std::string&)                                { return false; }
bool is_directory    (const std::string&)                                { return false; }
bool ensure_directory(const std::string&)                                { return false; }
bool remove_file     (const std::string&)                                { return true;  }

} // namespace ergo::io

namespace ergo::io::path {

std::string parent_of    (const std::string&)                 { return {}; }
std::string join         (const std::string& a, const std::string&) { return a; }
std::string extension_of (const std::string&)                 { return {}; }
std::string stem_of      (const std::string&)                 { return {}; }

} // namespace ergo::io::path
