#include "cli_args.h"

#include <string>
#include <vector>

namespace ergo::package_cli {

namespace {

/// コマンドの表。 解析 (許す選択肢)・使い方 (usage)・コマンドごとの --help (usage_for) はこれだけを読むので、
/// 受け付ける選択肢と使い方の表示がずれない。 --host / --ergo は全コマンド共通 (表には書かない)。
struct Command {
    std::string              name;
    std::string              synopsis;  ///< "ergo_pkg <name> " の後ろ ("\n" で続け行)
    std::string              summary;   ///< 1 行の説明
    std::vector<std::string> values;    ///< 値を取る選択肢
    std::vector<std::string> flags;     ///< 旗
    bool                     names = false;  ///< パッケージ名を取る
    bool                     watch = false;  ///< ergo_package_watch が要る (ERGO_BUILD_PACKAGE_WATCH)
};

const std::vector<Command>& commands() {
    static const std::vector<Command> kCommands = {
        {"init", "[--code <c>] [--upm]",
         "write the host declaration, cmake/ergo-bootstrap.cmake and .ergo/ in .gitignore "
         "(--upm: only .ergo/host.json, for a Unity project)",
         {"--code"}, {"--upm"}},
        {"list", "[--json] [--build <dir>]", "list Ergo itself, shared, private and UPM packages",
         {"--build"}, {"--json"}},
        {"doctor", "[--strict] [--json]",
         "check the declaration, the bootstrap's age and leaks (--strict: warnings are errors)",
         {}, {"--strict", "--json"}},
        {"fetch", "[<name>...] [--build <dir> | --cache <dir>] [--cmake <exe>]",
         "fetch link packages (default: host.use) at their pin, so an ERGO_PACKAGES_FETCH=ON configure "
         "needs no network",
         {"--build", "--cache", "--cmake"}, {}, true},
        {"add", "<name> [--optional] [--features <f>] [--kind link|tool] [--code <c>]\n"
                "        [--repository <url> --rev <sha|tag>] [--wiring <path>] [--allow-off-main] [--cache <dir>]",
         "add a package to host.use; a name not in the Ergo manifest becomes a private package "
         "(a tool goes to packages only)",
         {"--features", "--kind", "--code", "--repository", "--rev", "--wiring", "--cache"},
         {"--optional", "--allow-off-main"}, true},
        {"remove", "<name>", "remove a package from host.use / optional / features and private packages",
         {}, {}, true},
        {"update", "<name> --to <tag|main|sha|branch> [--allow-off-main] [--cache <dir>]",
         "pin a package to a 40-hex SHA in the file that owns it (rev and expect_tag only)",
         {"--to", "--cache"}, {"--allow-off-main"}, true},
        {"outdated", "[--json] [--fail-on tag|track|off-main] [--cache <dir>] [--root <dir>]",
         "check the pins for updates now (no state; --fail-on: exit 1 on that kind)",
         {"--cache", "--root", "--fail-on"}, {"--json"}, false, true},
        {"watch", "--host <dir>... [--state <dir>] [--out <dir>] [--cache <dir>] [--root <dir>]\n"
                  "        [--revisor-url <loopback url>] [--concordia-url <loopback url>]",
         "periodic check: new events to <out>/events.json, a report per host in <host>/.ergo/watch-report.json",
         {"--state", "--out", "--cache", "--root", "--revisor-url", "--concordia-url"}, {}, false, true},
        {"status", "--build <dir> [--junit <ctest junit xml>] [--out <file>]",
         "write <build>/ergo-packages.status.json, the host state ergo_package_viz --view host draws",
         {"--build", "--junit", "--out"}, {}},
    };
    return kCommands;
}

const Command* find_command(const std::string& name) {
    for (const auto& c : commands()) {
        if (c.name == name) return &c;
    }
    return nullptr;
}

bool contains(const std::vector<std::string>& v, const std::string& x) {
    for (const auto& e : v) {
        if (e == x) return true;
    }
    return false;
}

/// 1 コマンドの使い方の行 (共通の --host / --ergo を足す。 watch は --host を自分で持つ)。
std::string usage_line(const Command& c) {
    std::string line = "  ergo_pkg " + c.name + " " + c.synopsis;
    line += c.synopsis.find("--host") == std::string::npos ? " [--host <dir>] [--ergo <dir>]\n" : " [--ergo <dir>]\n";
    line += "      " + c.summary;
#if !defined(ERGO_PKG_CLI_WITH_WATCH)
    if (c.watch) line += " (needs ERGO_BUILD_PACKAGE_WATCH=ON)";
#endif
    return line + "\n";
}

const char* const kExitCodes =
    "exit codes: 0 ok / 1 findings to act on / 2 bad arguments or input / 3 cannot write or fetch\n";

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
    else if (key == "--junit") a.junit = value;
    else if (key == "--root") a.root = value;
    else if (key == "--fail-on") a.fail_on = value;
    else if (key == "--revisor-url") a.revisor_url = value;
    else if (key == "--concordia-url") a.concordia_url = value;
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
    const Command* command = find_command(out.command);
    if (!command) return fail("unknown command '" + out.command + "'");

    for (std::size_t i = 1; i < argv.size(); ++i) {
        const std::string& a = argv[i];
        if (a == "--help" || a == "-h") {
            out.help = true;
        } else if (a == "--host" || a == "--ergo" || contains(command->values, a)) {
            if (i + 1 >= argv.size()) return fail(a + " needs a value");
            assign(out, a, argv[++i]);
        } else if (contains(command->flags, a)) {
            set_flag(out, a);
        } else if (!a.empty() && a[0] == '-') {
            return fail("unknown option '" + a + "' for " + out.command);
        } else if (command->names) {
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

std::string usage() {
    std::string out = "usage:\n";
    for (const auto& c : commands()) out += usage_line(c);
    out += "common: --host <dir> (default .), --ergo <dir> (default $ERGO_SOURCE_DIR, then the Ergo that "
           "built this CLI)\n";
    out += "help of one command: ergo_pkg <command> --help\n";
    return out + kExitCodes;
}

std::string usage_for(const std::string& command) {
    const Command* c = find_command(command);
    if (!c) return usage();
    std::string out = "usage:\n" + usage_line(*c);
    out += "common: --host <dir> (default .), --ergo <dir> (default $ERGO_SOURCE_DIR, then the Ergo that "
           "built this CLI)\n";
    return out + kExitCodes;
}

} // namespace ergo::package_cli
