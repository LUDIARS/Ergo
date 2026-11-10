#include "ergo/studio/host/external_loader.h"

#include "abi_plugin.h"
#include "dynamic_library.h"

#include <algorithm>
#include <system_error>

namespace ergo::studio::host {
namespace fs = std::filesystem;

namespace {
#ifdef _WIN32
constexpr char kListSep = ';';
#else
constexpr char kListSep = ':';
#endif

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

void say(const LogFn& log, const char* level, const std::string& msg) {
    if (log) log(level, msg);
}
} // namespace

std::vector<fs::path> split_plugin_dirs(const std::string& raw) {
    std::vector<fs::path> out;
    std::size_t start = 0;
    while (start <= raw.size()) {
        const auto end  = raw.find(kListSep, start);
        const auto part = trim(raw.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (!part.empty()) out.push_back(fs::u8path(part));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return out;
}

std::vector<fs::path> find_pack_entries(const fs::path& pack_dir, const LogFn& log) {
    std::vector<fs::path> subdirs;
    std::error_code ec;
    fs::directory_iterator it(pack_dir, ec);
    if (ec) {
        say(log, "warn", "[ergo] plugin dir not readable: " + pack_dir.u8string() + " (" + ec.message() + ")");
        return {};
    }
    for (const auto& ent : it) {
        std::error_code e2;
        if (ent.is_directory(e2)) subdirs.push_back(ent.path());
    }
    std::sort(subdirs.begin(), subdirs.end(),
              [](const fs::path& a, const fs::path& b) { return a.filename().u8string() < b.filename().u8string(); });
    std::vector<fs::path> out;
    for (const auto& d : subdirs) {
        const fs::path f = d / ERGO_STUDIO_PLUGIN_FILE;
        std::error_code e2;
        if (fs::is_regular_file(f, e2)) out.push_back(f);
    }
    return out;
}

std::vector<ExternalFactory> load_external_factories(const std::string& raw, const LogFn& log) {
    std::vector<ExternalFactory> out;
    for (const auto& dir : split_plugin_dirs(raw)) {
        std::error_code ec;
        const fs::path abs = fs::absolute(dir, ec);
        for (const auto& file : find_pack_entries(ec ? dir : abs, log)) {
            const std::string name   = file.parent_path().filename().u8string();
            const std::string source = file.u8string();
            std::string err;
            auto lib = DynamicLibrary::open(file, err);
            if (!lib) {
                say(log, "error", "[ergo] external plugin '" + name + "' failed to load: " + err);
                continue;
            }
            auto fn = reinterpret_cast<ergo_studio_plugin_v1_create_fn>(lib->symbol(ERGO_STUDIO_PLUGIN_ENTRY));
            if (!fn) {
                say(log, "warn", "[ergo] external plugin '" + name + "': no " ERGO_STUDIO_PLUGIN_ENTRY " export — skipped");
                continue;
            }
            out.push_back({[lib, fn](const LogFn& l) { return AbiPlugin::create(lib, fn, l); }, source});
            say(log, "info", "[ergo] external plugin loaded: " + name + "  (" + source + ")");
        }
    }
    return out;
}

} // namespace ergo::studio::host
