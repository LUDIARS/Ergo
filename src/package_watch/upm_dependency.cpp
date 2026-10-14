#include "ergo/package_watch/upm_dependency.h"

#include "ergo/package_manager/declaration_check.h"

#include <algorithm>
#include <cctype>

namespace ergo::package_watch {

const char* const kUpmOwner = "upm";

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

/// Ergo 自身なら Eg、 同じリポを指す Ergo のマニフェストのパッケージのコード (末尾名を大小無視で比べる)。
/// どちらでもなければ末尾名。
std::string code_for(const std::string& repo_name, const pm::ErgoManifest* ergo) {
    if (lower(repo_name) == "ergo") return kErgoCode;
    if (ergo) {
        for (const auto& p : ergo->packages) {
            if (!p.code.empty() && lower(repository_name(p.repository)) == lower(repo_name)) return p.code;
        }
    }
    return repo_name;
}

} // namespace

std::string upm_repository(const std::string& url) {
    std::string s = url;
    const std::string git_plus = "git+";
    if (s.compare(0, git_plus.size(), git_plus) == 0) s.erase(0, git_plus.size());
    const auto query = s.find('?');
    if (query != std::string::npos) s.erase(query);
    return s;
}

std::vector<WatchDependency> upm_dependencies(const pm::UpmDeclaration& upm,
                                              const pm::ErgoManifest* ergo,
                                              std::vector<std::string>* skipped) {
    std::vector<WatchDependency> out;
    for (const auto& u : upm.dependencies) {
        if (!pm::is_full_sha(u.rev)) {
            if (skipped) {
                skipped->push_back(u.name + (u.rev.empty() ? std::string(": no #<sha> pin")
                                                           : ": #" + u.rev + " is a branch, not a 40-digit SHA"));
            }
            continue;
        }
        WatchDependency d;
        d.name         = u.name;
        d.repository   = upm_repository(u.url);
        d.code         = code_for(repository_name(d.repository), ergo);
        d.owner        = kUpmOwner;
        d.rev          = u.rev;
        d.track_branch = kDefaultTrackBranch;
        if (d.repository.empty() || d.code.empty()) {
            if (skipped) skipped->push_back(u.name + ": no repository");
            continue;
        }
        out.push_back(std::move(d));
    }
    std::sort(out.begin(), out.end(),
              [](const WatchDependency& a, const WatchDependency& b) { return a.name < b.name; });
    return out;
}

} // namespace ergo::package_watch
