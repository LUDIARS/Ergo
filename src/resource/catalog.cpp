#include "ergo/resource/catalog.h"

#include <algorithm>
#include <filesystem>
#include <set>

#include "ergo/common/json_min.h"
#include "ergo/io/file.h"

namespace ergo::resource {
namespace {

namespace jsonm = ergo::common::jsonm;
namespace fs    = std::filesystem;

/// Filename stem for the default entry name ("models/tree01.glb" → "tree01").
std::string stem_of(const std::string& path) {
    return fs::path(path).stem().string();
}

/// Join a relative entry path with the manifest directory.
std::string resolve_entry_path(const std::string& raw,
                               const fs::path& manifest_dir) {
    fs::path p(raw);
    if (p.is_absolute()) return raw;
    return (manifest_dir / p).lexically_normal().string();
}

} // namespace

bool Catalog::load_manifest(const std::string& manifest_path) {
    last_error_.clear();

    std::string text;
    if (!ergo::io::read_file(manifest_path, text)) {
        last_error_ = "manifest unreadable: " + manifest_path;
        return false;
    }

    jsonm::JsonValue root;
    if (!jsonm::parse(text, root) || !root.is_object()) {
        last_error_ = "manifest is not valid JSON: " + manifest_path;
        return false;
    }

    const jsonm::JsonValue* resources = root.find("resources");
    if (resources == nullptr || !resources->is_array()) {
        last_error_ = "manifest has no \"resources\" array: " + manifest_path;
        return false;
    }

    const fs::path manifest_dir = fs::path(manifest_path).parent_path();
    std::size_t skipped = 0;

    for (const jsonm::JsonValue& item : *resources->a) {
        if (!item.is_object()) {
            ++skipped;
            continue;
        }

        const jsonm::JsonValue* path_value = item.find("path");
        const std::string raw_path =
            path_value != nullptr ? path_value->as_string() : "";
        if (raw_path.empty()) {
            ++skipped;
            continue;
        }

        Entry entry;
        entry.path = resolve_entry_path(raw_path, manifest_dir);

        const jsonm::JsonValue* id_value = item.find("id");
        entry.id = id_value != nullptr ? id_value->as_string() : "";
        if (entry.id.empty()) entry.id = raw_path;

        const jsonm::JsonValue* name_value = item.find("name");
        entry.name = name_value != nullptr ? name_value->as_string() : "";
        if (entry.name.empty()) entry.name = stem_of(raw_path);

        const jsonm::JsonValue* tags_value = item.find("tags");
        if (tags_value != nullptr && tags_value->is_array()) {
            for (const jsonm::JsonValue& tag : *tags_value->a) {
                const std::string s = tag.as_string();
                if (!s.empty()) entry.tags.push_back(s);
            }
        }

        entries_.push_back(std::move(entry));
    }

    if (skipped > 0) {
        last_error_ = "skipped " + std::to_string(skipped) +
                      " entr(ies) without \"path\": " + manifest_path;
    }
    return true;
}

void Catalog::add(Entry entry) {
    entries_.push_back(std::move(entry));
}

void Catalog::clear() {
    entries_.clear();
    last_error_.clear();
}

const Entry* Catalog::find_by_id(const std::string& id) const {
    const auto it = std::find_if(
        entries_.begin(), entries_.end(),
        [&](const Entry& e) { return e.id == id; });
    return it != entries_.end() ? &*it : nullptr;
}

std::vector<std::string> Catalog::vocabulary() const {
    std::set<std::string> unique;
    for (const Entry& entry : entries_) {
        for (const std::string& tag : entry.tags) {
            if (!tag.empty()) unique.insert(tag);
        }
    }
    return std::vector<std::string>(unique.begin(), unique.end());
}

} // namespace ergo::resource
