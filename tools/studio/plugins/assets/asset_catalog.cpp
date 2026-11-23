#include "asset_catalog.h"

#include "../common/safe_path.h"

#include <algorithm>
#include <cctype>
#include <chrono>

namespace ergo::studio::plugins::assets {
namespace fs = std::filesystem;

namespace {

std::string lower_ext(const fs::path& file) {
    std::string e = file.extension().u8string();
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return e;
}

std::string lower_name(const fs::path& file) {
    std::string n = file.filename().u8string();
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return n;
}

bool ends_with(const std::string& s, const std::string& suf) {
    return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

bool one_of(const std::string& e, std::initializer_list<const char*> list) {
    return std::any_of(list.begin(), list.end(), [&](const char* x) { return e == x; });
}

std::int64_t unix_seconds(fs::file_time_type t) {
    // file_clock -> system_clock without C++20 clock_cast: shift by "now" on both clocks.
    const auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        t - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::duration_cast<std::chrono::seconds>(sys.time_since_epoch()).count();
}

} // namespace

std::string kind_of(const fs::path& file) {
    const std::string name = lower_name(file);
    if (ends_with(name, ".scene.json")) return "scene";
    const std::string e = lower_ext(file);
    if (one_of(e, {".gltf", ".glb", ".fbx", ".obj", ".vrm", ".pmx", ".dae", ".usd", ".usdz"})) return "model";
    if (one_of(e, {".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp", ".tga", ".dds", ".ktx", ".ktx2", ".hdr", ".exr", ".psd"}))
        return "texture";
    if (one_of(e, {".wav", ".ogg", ".mp3", ".flac", ".acb", ".awb", ".opus"})) return "audio";
    if (one_of(e, {".vert", ".frag", ".comp", ".geom", ".spv", ".hlsl", ".glsl", ".wgsl"})) return "shader";
    if (one_of(e, {".prefab", ".unity", ".scene", ".fieldmap"})) return "scene";
    if (one_of(e, {".json", ".yaml", ".yml", ".txt", ".csv", ".md", ".balance", ".look", ".xml", ".ini", ".toml"})) return "data";
    return "other";
}

std::optional<std::string> preview_type(const fs::path& file) {
    if (lower_name(file).empty()) return std::nullopt;
    const std::string e = lower_ext(file);
    if (e == ".png") return std::string("image/png");
    if (e == ".jpg" || e == ".jpeg") return std::string("image/jpeg");
    if (e == ".gif") return std::string("image/gif");
    if (e == ".webp") return std::string("image/webp");
    if (e == ".bmp") return std::string("image/bmp");
    // SVG is not previewed (it can carry script). Text is sent as plain text only.
    if (kind_of(file) == "data" || kind_of(file) == "shader" || e == ".gltf") return std::string("text/plain; charset=utf-8");
    return std::nullopt;
}

std::optional<AssetListing> list_folder(const fs::path& root, const std::string& rel, std::string& err) {
    const auto dir = resolve_within(root, rel);
    if (!dir) {
        err = "path is outside the asset folder";
        return std::nullopt;
    }
    std::error_code ec;
    if (!fs::is_directory(*dir, ec)) {
        err = "not a folder";
        return std::nullopt;
    }
    AssetListing out;
    for (fs::directory_iterator it(*dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end;
         it.increment(ec)) {
        if (out.entries.size() >= kMaxListing) {
            out.truncated = true;
            break;
        }
        const fs::path& p = it->path();
        const std::string name = p.filename().u8string();
        if (name.empty() || name[0] == '.') continue;   // .git, .worktrees, editor state
        AssetEntry e;
        e.name = name;
        e.path = relative_slash(root, p);
        std::error_code fe;
        e.directory = it->is_directory(fe);
        if (e.directory) {
            e.kind = "dir";
        } else if (it->is_regular_file(fe)) {
            e.kind = kind_of(p);
            e.size = it->file_size(fe);
            if (fe) e.size = 0;
        } else {
            continue;   // sockets, broken links
        }
        std::error_code te;
        const auto t = it->last_write_time(te);
        e.modified = te ? 0 : unix_seconds(t);
        out.entries.push_back(std::move(e));
    }
    if (ec && out.entries.empty()) {
        err = "cannot read the folder";
        return std::nullopt;
    }
    std::sort(out.entries.begin(), out.entries.end(), [](const AssetEntry& a, const AssetEntry& b) {
        if (a.directory != b.directory) return a.directory;
        return a.name < b.name;
    });
    return out;
}

JsonValue listing_json(const std::string& rel, const AssetListing& listing) {
    JsonValue entries = JsonValue::make_array();
    for (const auto& e : listing.entries) {
        entries.push(jobj({{"name", jstr(e.name)},
                           {"path", jstr(e.path)},
                           {"type", jstr(e.directory ? "dir" : "file")},
                           {"kind", jstr(e.kind)},
                           {"size", jnum(static_cast<double>(e.size))},
                           {"modified", jnum(static_cast<double>(e.modified))},
                           {"preview", jbool(!e.directory && preview_type(fs::u8path(e.name)).has_value())}}));
    }
    return jobj({{"dir", jstr(rel)}, {"truncated", jbool(listing.truncated)}, {"entries", entries}});
}

} // namespace ergo::studio::plugins::assets
