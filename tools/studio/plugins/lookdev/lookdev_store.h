#pragma once
/// Where look-dev files live: presets ("name=value" lines, <name>.look) under
/// the Studio home, and one folder per capture under the system temp
/// directory (Concordia accepts attachments from there, so a capture can be
/// posted as it is).
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::lookdev {

struct PresetEntry {
    std::string name;
    std::string updated;   ///< ISO 8601 UTC
};

class LookdevStore {
public:
    /// `presets_dir` and `captures_root` are created on first use.
    LookdevStore(std::filesystem::path presets_dir, std::filesystem::path captures_root);

    const std::filesystem::path& presets_dir() const { return presets_dir_; }
    const std::filesystem::path& captures_root() const { return captures_root_; }

    /// Newest first.
    std::vector<PresetEntry> presets() const;
    /// The preset's text; empty with `err` set when it cannot be read.
    std::optional<std::string> load(const std::string& name, std::string& err) const;
    bool save(const std::string& name, const std::string& text, std::string& err) const;

    /// A new, empty folder for one capture (named by the UTC time); empty with `err` on failure.
    std::optional<std::filesystem::path> new_capture_dir(std::string& err) const;
    /// Whether `file` is inside the captures root (only those may be posted).
    bool is_capture(const std::filesystem::path& file) const;

    /// A preset name usable as a file name: letters, digits, '-', '_', '.' and
    /// non-ASCII text; anything else (separators, ':' ...) refuses the name.
    static bool valid_name(const std::string& name);

private:
    std::filesystem::path presets_dir_;
    std::filesystem::path captures_root_;
};

/// The system temp directory with its long path names (Concordia's attachment
/// guard compares long paths; GetTempPath may return 8.3 short names).
/// Empty when it cannot be resolved.
std::filesystem::path long_temp_directory();

} // namespace ergo::studio::plugins::lookdev
