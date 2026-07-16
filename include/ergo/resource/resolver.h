#pragma once

/// Resolver — pick catalog entries by wanted tags (+ optional name hint).
///
/// Free functions over a `Catalog`; scoring is delegated to `tag_scorer`.
/// Returned `Candidate::entry` pointers point into the catalog and are
/// invalidated when the catalog is modified.

#include <cstddef>
#include <string>
#include <vector>

#include "ergo/resource/catalog.h"

namespace ergo::resource {

struct Candidate {
    const Entry* entry = nullptr;
    int          score = 0;
};

/// All entries with score > 0, ordered by score desc then name asc,
/// truncated to `max_candidates`.
std::vector<Candidate> resolve_candidates(
    const Catalog& catalog,
    const std::vector<std::string>& wanted_tags,
    const std::string& name_hint,
    std::size_t max_candidates = 10);

/// Best candidate or nullptr when nothing scores above 0.
const Entry* resolve_best(const Catalog& catalog,
                          const std::vector<std::string>& wanted_tags,
                          const std::string& name_hint);

} // namespace ergo::resource
