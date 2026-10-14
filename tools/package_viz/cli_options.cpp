#include "cli_options.h"

namespace ergo::package_viz::cli {

namespace {

bool fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

} // namespace

const char* usage() {
    return "usage:\n"
           "  ergo_package_viz <build-dir> --view packages|targets [--focus X]... [--expand P]...\n"
           "                   [--include-tests] [--config NAME] [--layers FILE] [--resolved FILE]\n"
           "                   [--no-version-probe] --out FILE.tela\n"
           "  ergo_package_viz <build-dir> --check [--include-tests] [--config NAME]\n"
           "\n"
           "  --view packages  P1: package versions (pin / SHA / features / marks)\n"
           "  --view targets   P2: target link graph grouped by .anatomia/layers.json\n"
           "  --focus X        keep nodes matching a package / layer / target and their neighbours\n"
           "  --expand P       targets view: do not collapse package P (host and ergo always expand)\n"
           "  --check          PKG-17: exit 1 when a real link is missing from the manifest\n"
           "Oversized views are reduced, collapsed and finally split into FILE.N.tela plus\n"
           "FILE.index.tela; a view that exceeds the Tela limits is never written.\n";
}

bool parse_options(const std::vector<std::string>& args, Options& o, std::string* error) {
    bool have_view = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        auto value = [&](std::string& dst) {
            if (i + 1 >= args.size()) return fail(error, a + " needs a value");
            dst = args[++i];
            return true;
        };
        if (a == "-h" || a == "--help") {
            o.help = true;
            return true;
        } else if (a == "--view") {
            std::string v;
            if (!value(v)) return false;
            if (v == "packages") {
                o.view.view = View::Packages;
            } else if (v == "targets") {
                o.view.view = View::Targets;
            } else if (v == "usage") {
                o.view.view = View::Usage;
            } else {
                return fail(error, "unknown view '" + v + "' (packages|targets|usage)");
            }
            have_view = true;
        } else if (a == "--focus") {
            std::string v;
            if (!value(v)) return false;
            o.view.focus.push_back(v);
        } else if (a == "--expand") {
            std::string v;
            if (!value(v)) return false;
            o.view.expand.push_back(v);
        } else if (a == "--out") {
            if (!value(o.out)) return false;
        } else if (a == "--config") {
            if (!value(o.config)) return false;
        } else if (a == "--layers") {
            if (!value(o.layers)) return false;
        } else if (a == "--resolved") {
            if (!value(o.resolved)) return false;
        } else if (a == "--root") {
            if (!value(o.root)) return false;
        } else if (a == "--manifest") {
            if (!value(o.manifest)) return false;
        } else if (a == "--scan") {
            o.scan = true;
        } else if (a == "--include-tests") {
            o.view.include_tests = true;
        } else if (a == "--no-version-probe") {
            o.version_probe = false;
        } else if (a == "--check") {
            o.check = true;
        } else if (!a.empty() && a[0] == '-') {
            return fail(error, "unknown option " + a);
        } else if (o.build_dir.empty()) {
            o.build_dir = a;
        } else {
            return fail(error, "unexpected argument " + a);
        }
    }
    if (o.build_dir.empty()) return fail(error, "missing <build-dir>");
    if (o.check && o.scan) return fail(error, "--check and --scan are exclusive");
    if (o.check || o.scan) return true;
    if (!have_view) return fail(error, "missing --view packages|targets|usage");
    if (o.out.empty()) return fail(error, "missing --out FILE.tela");
    return true;
}

} // namespace ergo::package_viz::cli
