// process.cpp がシェルを通さず argv のまま子を起動することの確認。
// 使い方: test_package_cli_process <package_cli_echo_args のパス>
#include "process.h"
#include "process_launch.h"
#include "windows_command_line.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect(bool ok, const std::string& what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << "\n";
        ++failures;
    }
}

void expect_eq(const std::string& got, const std::string& want, const std::string& what) {
    if (got != want) {
        std::cerr << "FAIL: " << what << "\n  got:  " << got << "\n  want: " << want << "\n";
        ++failures;
    }
}

void check_windows_quoting() {
    using ergo::package_cli::quote_windows_arg;
    using ergo::package_cli::windows_command_line;
    expect_eq(quote_windows_arg("plain"), "plain", "plain arg stays bare");
    expect_eq(quote_windows_arg(""), "\"\"", "empty arg is an empty pair of quotes");
    expect_eq(quote_windows_arg("a b"), "\"a b\"", "space is quoted");
    expect_eq(quote_windows_arg("a\"b"), "\"a\\\"b\"", "inner quote is escaped");
    expect_eq(quote_windows_arg("C:\\dir with space\\"), "\"C:\\dir with space\\\\\"", "trailing backslash doubles");
    expect_eq(quote_windows_arg("x\\\"y"), "\"x\\\\\\\"y\"", "backslashes before a quote double");
    expect_eq(quote_windows_arg("x&y|z<w>^%"), "x&y|z<w>^%", "shell metacharacters are plain characters");
    expect_eq(windows_command_line({"git", "-C", "a b"}), "git -C \"a b\"", "command line joins quoted args");
}

void check_arguments_reach_child(const std::string& echo) {
    using ergo::package_cli::capture_status;
    const std::vector<std::string> args = {"a b", "x&y", "p|q", "<in>", "it's", "^caret", "(paren)", "semi;colon",
                                           "two  spaces", "path with space\\", "*.cpp"};
    std::vector<std::string> argv = {echo};
    argv.insert(argv.end(), args.begin(), args.end());
    std::string out;
    const int rc = capture_status(argv, out);
    std::string want;
    for (const auto& a : args) want += "[" + a + "]\n";
    expect(rc == 0, "capture_status exit code 0");
    expect_eq(out, want, "each argument reaches the child unchanged, one by one");
}

void check_native_quotes(const std::string& echo) {
    using ergo::package_cli::launch;
    using ergo::package_cli::StreamMode;
    std::string out;
    const int rc = launch({echo, "a\"b", "x\\\"y", ""}, StreamMode::capture, StreamMode::discard, &out);
    expect(rc == 0, "native launch accepts quoted and empty arguments");
    expect_eq(out, "[a\"b]\n[x\\\"y]\n[]\n", "native launch preserves the original quotes and argument boundaries");
}

void check_exit_codes_and_streams(const std::string& echo) {
    using namespace ergo::package_cli;
    std::string out;
    expect(capture_status({echo, "--exit", "7", "x"}, out) == 7, "capture_status returns the child's exit code");
    expect_eq(out, "[x]\n", "output is captured even on a non-zero exit");
    expect(!capture({echo, "--exit", "1"}, out), "capture is false on a non-zero exit");

    expect(capture({echo, "--stderr", "noise", "kept"}, out), "capture succeeds with stderr output");
    expect_eq(out, "[kept]\n", "capture takes stdout only and drops stderr");

    expect(status({echo, "--exit", "3", "--stderr", "noise", "dropped"}) == 3, "status returns the exit code");
    expect(status({echo, "quiet"}) == 0, "status returns 0 on success");

    expect(run({echo, "--stderr", "run-stderr", "run-ok"}), "run is true on exit 0");
    expect(!run({echo, "--exit", "2"}), "run is false on a non-zero exit");
}

void check_refusals(const std::string& echo) {
    using namespace ergo::package_cli;
    std::string out = "stale";
    expect(capture_status({echo, "a\"b"}, out) == -1, "quote_safe still refuses a double quote");
    expect(out.empty(), "refused capture clears the output");
    expect(capture_status({echo, "$HOME"}, out) == -1, "quote_safe still refuses $");
    expect(capture_status({echo, std::string("a\0b", 3)}, out) == -1, "embedded NUL is rejected");
    expect(status({}) == -1, "empty argv is refused");
    expect(!run({"ergo-pkg-no-such-program-xyz"}), "missing program does not run");
    expect(status({"ergo-pkg-no-such-program-xyz"}) == -1, "missing program reports -1");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: test_package_cli_process <package_cli_echo_args>\n";
        return 2;
    }
    const std::string echo = argv[1];
    check_windows_quoting();
    check_arguments_reach_child(echo);
    check_native_quotes(echo);
    check_exit_codes_and_streams(echo);
    check_refusals(echo);
    if (failures != 0) return 1;
    std::cout << "test_package_cli_process: ok\n";
    return 0;
}
