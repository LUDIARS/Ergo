#include "ergo/resource/loader.h"

#include "ergo/io/file.h"
#include "ergo/resource/resolver.h"

namespace ergo::resource {

bool load(const Entry& entry, LoadedResource& out) {
    out.entry = entry;
    if (!ergo::io::read_file_bytes(entry.path, out.bytes)) {
        out = LoadedResource{};
        return false;
    }
    return true;
}

bool resolve_and_load(const Catalog& catalog,
                      const std::vector<std::string>& wanted_tags,
                      const std::string& name_hint,
                      LoadedResource& out) {
    const Entry* best = resolve_best(catalog, wanted_tags, name_hint);
    if (best == nullptr) {
        out = LoadedResource{};
        return false;
    }
    return load(*best, out);
}

} // namespace ergo::resource
