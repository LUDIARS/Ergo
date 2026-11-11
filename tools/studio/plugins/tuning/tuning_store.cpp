#include "tuning_store.h"

#include "../common/json_parse.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace ergo::studio::plugins::tuning {
namespace fs = std::filesystem;

namespace {

constexpr std::size_t kMaxSlug = 40;

std::string text_of(const JsonValue& v, const char* key) {
    const JsonValue* f = field(v, key);
    return f != nullptr && f->is_string() ? f->as_string() : std::string();
}

std::optional<JsonValue> read_json(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream s;
    s << in.rdbuf();
    return parse_json(s.str());
}

/// "2026-11-11T08:00:00Z" -> "20261111-080000".
std::string stamp_of(const std::string& iso) {
    std::string digits;
    for (char c : iso) {
        if (std::isdigit(static_cast<unsigned char>(c))) digits += c;
    }
    if (digits.size() < 14) digits.resize(14, '0');
    return digits.substr(0, 8) + "-" + digits.substr(8, 6);
}

} // namespace

bool safe_part(const std::string& part) {
    return !part.empty() && part.size() <= 120 && part != "." && part != ".." &&
           std::all_of(part.begin(), part.end(), [](char c) {
               return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.';
           });
}

std::string slug_of(const std::string& name) {
    std::string out;
    for (char c : name) {
        const bool keep = std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_';
        if (keep) out += c;
        else if (!out.empty() && out.back() != '-') out += '-';
        if (out.size() >= kMaxSlug) break;
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "version" : out;
}

JsonValue diff_values(const JsonValue& before, const JsonValue& after) {
    JsonValue out = JsonValue::make_array();
    if (!after.is_object()) return out;
    for (const auto& [name, value] : *after.o) {
        const JsonValue* old = before.is_object() ? field(before, name) : nullptr;
        if (old != nullptr && ergo::common::jsonm::serialize(*old) == ergo::common::jsonm::serialize(value)) continue;
        out.push(jobj({{"name", jstr(name)}, {"before", old != nullptr ? *old : JsonValue::make_null()}, {"after", value}}));
    }
    return out;
}

fs::path TuningStore::folder(const std::string& app, const fs::path& profile_dir) const {
    if (!profile_dir.empty()) return profile_dir;
    if (fallback_root_.empty()) return {};
    return fallback_root_ / fs::u8path(app);
}

std::vector<VersionEntry> TuningStore::list(const fs::path& folder) const {
    std::vector<VersionEntry> out;
    std::error_code ec;
    if (folder.empty() || !fs::is_directory(folder, ec)) return out;
    for (const auto& entry : fs::directory_iterator(folder, ec)) {
        if (!entry.is_regular_file(ec) || entry.path().extension() != ".json") continue;
        const auto json = read_json(entry.path());
        if (!json) continue;
        VersionEntry v;
        v.id = entry.path().stem().u8string();
        v.name = text_of(*json, "name");
        v.note = text_of(*json, "note");
        v.created = text_of(*json, "created");
        if (const JsonValue* values = field(*json, "values"); values != nullptr && values->is_object()) v.values = values->o->size();
        out.push_back(std::move(v));
    }
    std::sort(out.begin(), out.end(), [](const VersionEntry& a, const VersionEntry& b) { return a.id > b.id; });
    return out;
}

std::optional<JsonValue> TuningStore::load(const fs::path& folder, const std::string& id, std::string& err) const {
    if (!safe_part(id)) {
        err = "bad version id";
        return std::nullopt;
    }
    auto json = read_json(folder / fs::u8path(id + ".json"));
    if (!json) err = "no such version: " + id;
    return json;
}

std::optional<std::string> TuningStore::save(const fs::path& folder, const std::string& app, const std::string& name,
                                             const std::string& note, const JsonValue& values, const std::string& created,
                                             std::string& err) const {
    if (folder.empty()) {
        err = "no folder for the versions (the Studio home is unknown and no profile names one)";
        return std::nullopt;
    }
    if (name.empty()) {
        err = "a version needs a name";
        return std::nullopt;
    }
    if (!values.is_object() || values.o->empty()) {
        err = "no values to save (is the game connected?)";
        return std::nullopt;
    }
    std::error_code ec;
    fs::create_directories(folder, ec);
    if (ec) {
        err = "cannot create " + folder.u8string() + ": " + ec.message();
        return std::nullopt;
    }
    std::string id = stamp_of(created) + "-" + slug_of(name);
    for (int n = 2; fs::exists(folder / fs::u8path(id + ".json"), ec); ++n) id = stamp_of(created) + "-" + slug_of(name) + "-" + std::to_string(n);
    const JsonValue doc = jobj({{"app", jstr(app)}, {"name", jstr(name)}, {"note", jstr(note)}, {"created", jstr(created)}, {"values", values}});
    std::ofstream out(folder / fs::u8path(id + ".json"), std::ios::binary);
    out << ergo::common::jsonm::serialize(doc) << "\n";
    if (!out) {
        err = "cannot write the version";
        return std::nullopt;
    }
    return id;
}

} // namespace ergo::studio::plugins::tuning
