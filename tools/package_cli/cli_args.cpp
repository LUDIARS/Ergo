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
    } else {
        return false;
    }
    return true;
}

void assign(Args& a, const std::string& key, const std::string& value) {
    if (key == "--host") a.host = value;
    else if (key == "--ergo") a.ergo = value;
    else if (key == "--code") a.code = value;
    else if (key == "--build") a.build = value;
    else if (key == "--cache") a.cache = value;
    else if (key == "--cmake") a.cmake = value;
}

void set_flag(Args& a, const std::string& key) {
    if (key == "--upm") a.upm = true;
    else if (key == "--json") a.json = true;
    else if (key == "--strict") a.strict = true;
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
    return true;
}

const char* usage() {
    return "usage:\n"
           "  ergo_pkg init   [--code <c>] [--upm]                [--host <dir>] [--ergo <dir>]\n"
           "  ergo_pkg list   [--json] [--build <dir>]             [--host <dir>] [--ergo <dir>]\n"
           "  ergo_pkg doctor [--strict] [--json]                  [--host <dir>] [--ergo <dir>]\n"
           "  ergo_pkg fetch  [<name>...] [--build <dir> | --cache <dir>] [--cmake <exe>]\n"
           "                                                       [--host <dir>] [--ergo <dir>]\n"
           "exit codes: 0 ok / 1 findings to act on / 2 bad arguments or input / 3 cannot write or fetch\n";
}

} // namespace ergo::package_cli
