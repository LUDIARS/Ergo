#include "profile_store.h"

#include "../common/file_io.h"
#include "profile_schema.h"

#include <algorithm>
#include <system_error>

namespace ergo::studio::plugins::render_pipeline {
namespace fs = std::filesystem;
namespace {

const std::string kSuffix = ".profile.json";

bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

ProfileStore::Result fail(int status, std::string err) {
    ProfileStore::Result r;
    r.status = status;
    r.err    = std::move(err);
    return r;
}

} // namespace

JsonValue to_json_value(const ProfileListEntry& e) {
    return jobj({{"file", jstr(e.file)}, {"profile_name", jstr(e.profile_name)},
                 {"size", jnum(static_cast<double>(e.size))}, {"mtime", jstr(e.mtime)}});
}

bool is_safe_profile_file(const std::string& f) {
    return ends_with(f, kSuffix) && f.size() > kSuffix.size() && f.find('/') == std::string::npos &&
           f.find('\\') == std::string::npos && f.find("..") == std::string::npos && f.find(':') == std::string::npos;
}

std::vector<ProfileListEntry> ProfileStore::list() const {
    std::vector<ProfileListEntry> out;
    std::error_code ec;
    if (!configured() || !fs::is_directory(dir_, ec)) return out;
    for (const auto& ent : fs::directory_iterator(dir_, ec)) {
        const std::string name = ent.path().filename().u8string();
        std::error_code e2;
        if (!ends_with(name, kSuffix) || !ent.is_regular_file(e2)) continue;
        ProfileListEntry e;
        e.file         = name;
        e.profile_name = name.substr(0, name.size() - kSuffix.size());
        std::string err;
        if (const auto text = read_text_file(ent.path(), err)) {
            JsonValue v;
            if (ergo::common::jsonm::parse(*text, v)) {
                const JsonValue* pn = field(v, "profile_name");
                if (pn && pn->is_string() && !pn->s.empty()) e.profile_name = pn->s;
            }
        }
        e.size  = fs::file_size(ent.path(), e2);
        e.mtime = iso_utc(fs::last_write_time(ent.path(), e2));
        out.push_back(std::move(e));
    }
    std::sort(out.begin(), out.end(), [](const ProfileListEntry& a, const ProfileListEntry& b) { return a.file < b.file; });
    return out;
}

ProfileStore::Result ProfileStore::load(const std::string& file) const {
    if (!is_safe_profile_file(file)) return fail(400, "invalid profile file name: " + file);
    const fs::path full = dir_ / fs::u8path(file);
    std::error_code ec;
    if (!fs::exists(full, ec)) return fail(404, "profile not found: " + file);
    std::string err;
    const auto text = read_text_file(full, err);
    if (!text) return fail(400, err);
    JsonValue v;
    if (!ergo::common::jsonm::parse(*text, v)) return fail(400, "profile JSON parse failed (" + file + ")");
    Result r;
    r.ok      = true;
    r.file    = file;
    r.profile = normalize_profile(v);
    r.mtime   = iso_utc(fs::last_write_time(full, ec));
    return r;
}

ProfileStore::Result ProfileStore::save(const JsonValue& profile, const std::string& file) const {
    const JsonValue   normalized = normalize_profile(profile);
    const std::string target     = file.empty() ? profile_file_name(field(normalized, "profile_name")->s) : file;
    if (!is_safe_profile_file(target)) return fail(400, "invalid profile file name: " + target);
    const fs::path full = dir_ / fs::u8path(target);
    std::string err;
    if (!write_text_file(full, serialize_profile(normalized), err)) return fail(500, err);
    std::error_code ec;
    Result r;
    r.ok      = true;
    r.file    = target;
    r.profile = normalized;
    r.mtime   = iso_utc(fs::last_write_time(full, ec));
    return r;
}

} // namespace ergo::studio::plugins::render_pipeline
