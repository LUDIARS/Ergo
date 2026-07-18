#pragma once

/// `Catalog` — the set of resources tag-resolution selects from.
///
/// Entries come from JSON manifests (see `spec/module/resource.md` for the
/// format) or are added programmatically. Loading never throws; failures
/// return false and leave the reason in `last_error()` so callers can log
/// it (this module deliberately doesn't depend on ergo_log).

#include <string>
#include <vector>

#include "ergo/resource/entry.h"

namespace ergo::resource {

class Catalog {
 public:
    /// Parse a manifest JSON file and append its entries. Relative entry
    /// paths are resolved against the manifest's directory. Entries with
    /// a missing/empty "path" are skipped (counted in last_error()).
    /// Returns false (and appends nothing) when the file is unreadable
    /// or the JSON is malformed.
    bool load_manifest(const std::string& manifest_path);

    /// Append one entry as-is (no path resolution / no defaults).
    void add(Entry entry);

    void clear();

    /// All entries. Pointers into this vector (e.g. resolver candidates)
    /// are invalidated by load_manifest/add/clear.
    const std::vector<Entry>& entries() const { return entries_; }

    /// nullptr when no entry has the id. When more than one entry shares an
    /// id (e.g. two manifest items whose "id" was both omitted and both
    /// defaulted to the same path), the most recently added one wins — see
    /// load_manifest()'s duplicate-id diagnostic in last_error().
    const Entry* find_by_id(const std::string& id) const;

    /// Sorted, de-duplicated union of all entry tags (tag-palette source).
    std::vector<std::string> vocabulary() const;

    /// Reason of the most recent load_manifest failure ("" after success).
    const std::string& last_error() const { return last_error_; }

 private:
    std::vector<Entry> entries_;
    std::string last_error_;
};

} // namespace ergo::resource
