#include "cli_args.h"

#include <set>

namespace ergo::package_cli {

namespace {

/// コマンドごとに許す選択肢 (値を取るものと旗)。
struct Allowed {
    std::set<std::string> values;
    std::set<std::string> flags;
    bool                  names = false;
};

bool allowed_for(const std::string& command, Allowed& out) {
    const std::set<std::string> common = {"--host", "--ergo"};
    out.values = common;
    if (command == "init") {
        out.values.insert("--code");
        out.flags = {"--upm"};
    } else if (command == "list") {
        out.values.insert("--build");
        out.flags = {"--json"};
    } else if (command == "doctor") {
        out.flags = {"--strict", "--json"};
    } else if (command == "fetch") {
        out.values.insert({"--build", "--cache", "--cmake"});
        out.names = true;
    } else if (command == "add") {
        out.values.insert({"--features", "--kind", "--code", "--repository", "--rev", "--wiring", "--cache"});
        out.flags = {"--optional", "--allow-off-main"};
        out.names = true;
    } else if (command == "remove") {
        out.names = true;
    } else if (command == "update") {
        out.values.insert({"--to", "--cache"});
        out.flags = {"--allow-off-main"};
        out.names = true;
    } else if (command == "outdated") {
        out.values.insert({"--cache", "--root", "--fail-on"});
        out.flags = {"--json"};
    } else if (command == "watch") {
        out.values.insert({"--state", "--out", "--cache", "--root"});
    } else {
        return false;
    }
    return true;
}

void assign(Args& a, const std::string& key, const std::string& value) {
    if (key == "--host") {
        a.host = value;
        a.hosts.push_back(value);
    }
    else if (key == "--ergo") {
        a.ergo          = value;
        a.ergo_explicit = true;
    }
    else if (key == "--code") a.code = value;
    else if (key == "--build") a.build = value;
    else if (key == "--cache") a.cache = value;
    else if (key == "--cmake") a.cmake = value;
    else if (key == "--features") a.features = value;
    else if (key == "--kind") a.kind = value;
    else if (key == "--repository") a.repository = value;
    else if (key == "--rev") a.rev = value;
    else if (key == "--wiring") a.wiring = value;
    else if (key == "--to") a.to = value;
    else if (key == "--state") a.state = value;
    else if (key == "--out") a.out = value;
    else if (key == "--root") a.root = value;
    else if (key == "--fail-on") a.fail_on = value;
}

void set_flag(Args& a, const std::string& key) {
    if (key == "--upm") a.upm = true;
    else if (key == "--json") a.json = true;
    else if (key == "--strict") a.strict = true;
    else if (key == "--optional") a.optional = true;
    else if (key == "--allow-off-main") a.allow_off_main = true;
}

} // namespace

bool parse_args(const std::vector<std::string>& argv, Args& out, std::string* error) {
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return false;
    };
    if (argv.empty()) return fail("no command");
    if (argv[0] == "--help" || argv[0] == "-h" || argv[0] == "help") {
        out.help = true;
        return true;
    }
    out.command = argv[0];
    Allowed allowed;
    if (!allowed_for(out.command, allowed)) return fail("unknown command '" + out.command + "'");

    for (std::size_t i = 1; i < argv.size(); ++i) {
        const std::string& a = argv[i];
        if (a == "--help" || a == "-h") {
            out.help = true;
        } else if (allowed.values.count(a)) {
            if (i + 1 >= argv.size()) return fail(a + " needs a value");
            assign(out, a, argv[++i]);
        } else if (allowed.flags.count(a)) {
            set_flag(out, a);
        } else if (!a.empty() && a[0] == '-') {
            return fail("unknown option '" + a + "' for " + out.command);
        } else if (allowed.names) {
            out.names.push_back(a);
        } else {
            return fail("unexpected argument '" + a + "' for " + out.command);
        }
    }
    if (!out.build.empty() && !out.cache.empty()) return fail("--build and --cache are exclusive");
    const bool one_name = out.command == "add" || out.command == "remove" || out.command == "update";
    if (one_name && !out.help && out.names.size() != 1) return fail(out.command + " takes exactly one package name");
    if (out.command == "update" && !out.help && out.to.empty()) return fail("update needs --to <tag|main|sha>");
    return true;
}

const char* usage() {
    return "usage:\n"
           "  ergo_pkg init   [--code <c>] [--upm]                [--host <dir>] [--ergo <dir>]\n"
           "  ergo_pkg list   [--json] [--build <dir>]             [--host <dir>] [--ergo <dir>]\n"
           "  ergo_pkg doctor [--strict] [--json]                  [--host <dir>] [--ergo <dir>]\n"
           "  ergo_pkg fetch  [<name>...] [--build <dir> | --cache <dir>] [--cmake <exe>]\n"
           "                                                       [--host <dir>] [--ergo <dir>]\n"
           "  ergo_pkg outdated [--json] [--fail-on tag|track|off-main] [--cache <dir>] [--root <dir>]\n"
           "                                                       [--host <dir>] [--ergo <dir>]\n"
           "  ergo_pkg watch  --host <dir>... [--state <dir>] [--out <dir>] [--cache <dir>] [--root <dir>]\n"
           "                                                       [--ergo <dir>]\n"
           "exit codes: 0 ok / 1 findings to act on / 2 bad arguments or input / 3 cannot write or fetch\n";
}

} // namespace ergo::package_cli
