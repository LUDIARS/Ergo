#pragma once
/// Visus files (`*.visus.json`, Pictor render-definition recipes) under the
/// configured project roots: scan, load, save. Pure file logic, no sockets.
///
/// The roots come from the caller (the Studio reads VISUS_PROJECT_ROOTS,
/// ";"-separated). Unlike the TS plugin there is no fallback to the working
/// directory: with no roots every call reports that the roots are unset.
#include "../common/json_util.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::visus {

struct VisusSummary {
    std::string rel_path;          ///< "/" separated, relative to the root
    std::string abs_path;
    std::string name;
    std::string geometry_kind;     ///< "none" when absent
    std::size_t materials_count = 0;
    uintmax_t   bytes = 0;
    std::string mtime;             ///< ISO-8601 UTC
};
JsonValue to_json_value(const VisusSummary& s);

/// Split a VISUS_PROJECT_ROOTS value (";" separated, blanks dropped).
std::vector<std::filesystem::path> parse_roots(const std::string& raw);

class VisusStore {
public:
    explicit VisusStore(std::vector<std::filesystem::path> roots);

    const std::vector<std::filesystem::path>& roots() const { return roots_; }
    /// nullptr for an out-of-range index.
    const std::filesystem::path* root_at(double index) const;

    /// Every readable *.visus.json under the root, sorted by rel_path. Skips
    /// dot entries, node_modules, build and dist. Broken files are left out.
    std::vector<VisusSummary> scan(const std::filesystem::path& root) const;

    struct Result {
        bool        ok     = false;
        int         status = 200;   ///< HTTP status for the error
        std::string err;
        std::string content;        ///< load only
    };
    Result load(double root_index, const std::string& rel) const;
    /// `content` must be valid JSON and `rel` end with ".visus.json".
    Result save(double root_index, const std::string& rel, const std::string& content) const;

private:
    std::vector<std::filesystem::path> roots_;
};

} // namespace ergo::studio::plugins::visus
