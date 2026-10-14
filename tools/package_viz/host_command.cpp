#include "host_command.h"

namespace ergo::package_viz::cli {

namespace {

std::string join(const std::string& dir, const std::string& rel) {
    if (dir.empty()) return rel;
    const char last = dir.back();
    return (last == '/' || last == '\\') ? dir + rel : dir + "/" + rel;
}

} // namespace

package_model::HostStatePaths host_paths_from_options(const Options& o) {
    package_model::HostStatePaths p;
    p.resolved = o.resolved.empty() ? join(o.build_dir, "ergo-packages.resolved.json") : o.resolved;
    if (!o.report.empty()) {
        p.report = o.report;
    } else if (!o.host_dir.empty()) {
        p.report = join(o.host_dir, ".ergo/watch-report.json");
    }
    p.status = o.status.empty() ? join(o.build_dir, "ergo-packages.status.json") : o.status;
    return p;
}

} // namespace ergo::package_viz::cli
