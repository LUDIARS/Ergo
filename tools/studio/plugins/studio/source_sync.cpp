#include "source_sync.h"

#include <regex>

namespace ergo::studio::plugins::studio {
namespace {

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(" \t\r") - b + 1);
}

bool starts(const std::string& s, const char* p) { return s.rfind(p, 0) == 0; }

} // namespace

BranchStatus parse_branch_status(const std::string& text) {
    BranchStatus s;
    static const std::regex ab(R"(\+(\d+)\s+-(\d+))");
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        std::string line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        start = end == std::string::npos ? text.size() + 1 : end + 1;

        if (starts(line, "# branch.head ")) {
            const std::string head = trim(line.substr(14));
            s.branch = head == "(detached)" ? std::nullopt : std::optional<std::string>(head);
        } else if (starts(line, "# branch.upstream ")) {
            s.upstream = trim(line.substr(18));
        } else if (starts(line, "# branch.ab ")) {
            std::smatch m;
            if (std::regex_search(line, m, ab)) {
                s.ahead  = std::stoi(m[1]);
                s.behind = std::stoi(m[2]);
            }
        } else if (line.size() >= 2 && (line[0] == '1' || line[0] == '2' || line[0] == 'u') && line[1] == ' ') {
            ++s.changed;
        } else if (starts(line, "? ")) {
            ++s.untracked;
        }
    }
    return s;
}

FastForwardDecision decide_fast_forward(const BranchStatus& s, const std::string& base) {
    FastForwardDecision d;
    if (s.branch != base) {
        d.reason = base + " 以外のブランチ (" + s.branch.value_or("detached") + ") にいます";
    } else if (!s.upstream) {
        d.reason = "追跡ブランチがありません";
    } else if (s.changed > 0) {
        d.reason = "未コミットの変更が " + std::to_string(s.changed) + " 件あります";
    } else if (s.ahead > 0) {
        d.reason = "手元だけのコミットが " + std::to_string(s.ahead) + " 件あります";
    } else if (s.behind == 0) {
        d.reason = "すでに最新です";
    } else {
        d.allowed = true;
        d.branch  = base;
    }
    return d;
}

} // namespace ergo::studio::plugins::studio
