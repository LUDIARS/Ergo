#pragma once

/// ergo::resource — tag-driven resource selection & loading.
///
/// `Entry` is one selectable resource in a `Catalog`. The tag vocabulary
/// is shared with the Curare asset DB (and MUSA Clio on the Unity side);
/// this module itself never talks to the network.

#include <string>
#include <vector>

namespace ergo::resource {

struct Entry {
    /// Unique id. Defaults to `path` when the manifest omits it.
    std::string id;

    /// Match / display name. Defaults to the filename stem.
    std::string name;

    /// Resolved file path. Relative manifest paths are joined with the
    /// manifest's directory at load time, so this is always openable as-is.
    std::string path;

    /// Tags describing the resource (Curare vocabulary).
    std::vector<std::string> tags;
};

} // namespace ergo::resource
