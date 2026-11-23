#include "lookdev_store.h"

#include "../common/file_io.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <system_error>

namespace ergo::studio::plugins::lookdev {
namespace fs = std::filesystem;

namespace {

constexpr const char* kPresetExtension = ".look";

std::string utc_stamp() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    char text[32];
    std::strftime(text, sizeof(text), "%Y%m%d-%H%M%S", &utc);
    return text;
}

fs::path utf8_path(const std::string& text) { return fs::u8path(text); }

std::string utf8_text(const fs::path& p) { return p.u8string(); }

} // namespace

LookdevStore::LookdevStore(fs::path presets_dir, fs::path captures_root)
    : presets_dir_(std::move(presets_dir)), captures_root_(std::move(captures_root)) {}

bool LookdevStore::valid_name(const std::string& name) {
    if (name.empty() || name.size() > 80 || name.front() == '.') return false;
    return std::all_of(name.begin(), name.end(), [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return u >= 0x80 || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
               c == '_' || c == '.' || c == ' ';
    });
}

std::vector<PresetEntry> LookdevStore::presets() const {
    std::vector<std::pair<fs::file_time_type, PresetEntry>> found;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(presets_dir_, ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != kPresetExtension) continue;
        const auto when = entry.last_write_time(ec);
        found.push_back({when, {utf8_text(entry.path().stem()), iso_utc(when)}});
    }
    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<PresetEntry> out;
    for (auto& [when, entry] : found) out.push_back(std::move(entry));
    return out;
}

std::optional<std::string> LookdevStore::load(const std::string& name, std::string& err) const {
    if (!valid_name(name)) {
        err = "invalid preset name";
        return std::nullopt;
    }
    return read_text_file(presets_dir_ / utf8_path(name + kPresetExtension), err);
}

bool LookdevStore::save(const std::string& name, const std::string& text, std::string& err) const {
    if (!valid_name(name)) {
        err = "invalid preset name";
        return false;
    }
    std::error_code ec;
    fs::create_directories(presets_dir_, ec);
    if (ec) {
        err = "cannot create " + utf8_text(presets_dir_) + ": " + ec.message();
        return false;
    }
    return write_text_file(presets_dir_ / utf8_path(name + kPresetExtension), text, err);
}

std::optional<fs::path> LookdevStore::new_capture_dir(std::string& err) const {
    const std::string stamp = utc_stamp();
    std::error_code ec;
    // Two captures in one second get a numbered second folder.
    for (int n = 0; n < 100; ++n) {
        const fs::path dir = captures_root_ / (n == 0 ? stamp : stamp + "-" + std::to_string(n));
        if (fs::exists(dir, ec)) continue;
        fs::create_directories(dir, ec);
        if (ec) {
            err = "cannot create " + utf8_text(dir) + ": " + ec.message();
            return std::nullopt;
        }
        return dir;
    }
    err = "too many captures in one second";
    return std::nullopt;
}

bool LookdevStore::is_capture(const fs::path& file) const {
    std::error_code ec;
    const fs::path root = fs::weakly_canonical(captures_root_, ec);
    if (ec) return false;
    const fs::path target = fs::weakly_canonical(file, ec);
    if (ec) return false;
    const auto [root_end, unused] = std::mismatch(root.begin(), root.end(), target.begin(), target.end());
    (void)unused;
    return root_end == root.end() && target != root;
}

fs::path long_temp_directory() {
    std::error_code ec;
    const fs::path temp = fs::temp_directory_path(ec);
    if (ec) return {};
    // canonical resolves through the file system, which spells out 8.3 short names.
    const fs::path resolved = fs::canonical(temp, ec);
    return ec ? fs::path() : resolved;   // empty: the caller reports the temp directory as unusable
}

} // namespace ergo::studio::plugins::lookdev
