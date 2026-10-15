#include "git_remote_source.h"
#include "process.h"

#include <iostream>
#include <string>
#include <vector>

namespace {
std::vector<std::vector<std::string>> calls;
const std::string sha(40, 'a');
}

// Link-time process double: a rejected repository must never reach capture.
// quote_safe is permissive here to isolate list_refs' own validation.
namespace ergo::package_cli {
bool quote_safe(const std::string& value) { return !value.empty(); }
bool capture(const std::vector<std::string>& argv, std::string& out) {
    calls.push_back(argv);
    out = sha + "\trefs/heads/main\n" + sha + "\trefs/tags/v1.0.0\n";
    return true;
}
int capture_status(const std::vector<std::string>&, std::string&) { return -1; }
}

int main() {
    ergo::package_cli::GitRemoteSource source("");
    const auto rejected = source.list_refs({"example", "--upload-pack=unexpected"});
    if (rejected.observed || rejected.reason != "repository must not start with '-'" || !calls.empty()) {
        std::cerr << "leading dash must be rejected before capture\n";
        return 1;
    }
    for (const std::string repository : {"https://example.invalid/repo.git",
                                        "git@example.invalid:repo.git", "C:/local/repo", "./local/repo"}) {
        calls.clear();
        const auto listing = source.list_refs({"example", repository});
        const std::vector<std::string> expected = {"git", "ls-remote", "--tags", "--heads", "--", repository};
        if (calls.size() != 1 || calls.front() != expected || !listing.observed ||
            !listing.reason.empty() || listing.heads.at("main") != sha || listing.tags.at("v1.0.0") != sha) {
            std::cerr << "normal repository must retain ref parsing and follow option terminator\n";
            return 1;
        }
    }
    return 0;
}
