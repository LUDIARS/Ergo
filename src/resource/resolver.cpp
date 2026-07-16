#include "ergo/resource/resolver.h"

#include <algorithm>

#include "ergo/resource/tag_scorer.h"

namespace ergo::resource {

std::vector<Candidate> resolve_candidates(
    const Catalog& catalog,
    const std::vector<std::string>& wanted_tags,
    const std::string& name_hint,
    std::size_t max_candidates) {
    std::vector<Candidate> candidates;

    for (const Entry& entry : catalog.entries()) {
        const int s = score(entry.tags, wanted_tags, entry.name, name_hint);
        if (s <= 0) continue;
        candidates.push_back(Candidate{&entry, s});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                  if (a.score != b.score) return a.score > b.score;
                  return a.entry->name < b.entry->name;
              });

    if (candidates.size() > max_candidates) {
        candidates.resize(max_candidates);
    }
    return candidates;
}

const Entry* resolve_best(const Catalog& catalog,
                          const std::vector<std::string>& wanted_tags,
                          const std::string& name_hint) {
    const auto candidates =
        resolve_candidates(catalog, wanted_tags, name_hint, 1);
    return candidates.empty() ? nullptr : candidates.front().entry;
}

} // namespace ergo::resource
