#pragma once

/// Loader — read a resolved resource's bytes.
///
/// Decoding (image / model / audio) is out of scope: hosts hand
/// `LoadedResource::bytes` to their own decoders.

#include <cstdint>
#include <string>
#include <vector>

#include "ergo/resource/catalog.h"

namespace ergo::resource {

struct LoadedResource {
    Entry entry;                 // copy — safe after catalog mutation
    std::vector<uint8_t> bytes;  // whole file content
};

/// Read `entry.path` into `out`. Returns false (out cleared) when the
/// file is missing or unreadable.
bool load(const Entry& entry, LoadedResource& out);

/// resolve_best + load in one call. False when nothing matches or the
/// matched file can't be read.
bool resolve_and_load(const Catalog& catalog,
                      const std::vector<std::string>& wanted_tags,
                      const std::string& name_hint,
                      LoadedResource& out);

} // namespace ergo::resource
