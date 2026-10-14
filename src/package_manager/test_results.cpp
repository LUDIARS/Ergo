#include "ergo/package_manager/test_results.h"

#include <cstddef>

namespace ergo::package_manager {

namespace {

/// `<name` の直後が名前の終わり (空白・`>`・`/`) なら true (`<testcase` と `<testcases` を区別する)。
bool tag_at(const std::string& xml, std::size_t pos, const std::string& name) {
    const std::string open = "<" + name;
    if (xml.compare(pos, open.size(), open) != 0) return false;
    const std::size_t end = pos + open.size();
    if (end >= xml.size()) return false;
    const char c = xml[end];
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '>' || c == '/';
}

/// 開きタグ (`<` から `>` まで) の属性の値。 無ければ ""。
std::string attribute(const std::string& tag, const std::string& name) {
    const std::string key = " " + name + "=";
    std::size_t at = tag.find(key);
    if (at == std::string::npos) return {};
    at += key.size();
    if (at >= tag.size()) return {};
    const char quote = tag[at];
    if (quote != '"' && quote != '\'') return {};
    const std::size_t end = tag.find(quote, at + 1);
    return end == std::string::npos ? std::string{} : tag.substr(at + 1, end - at - 1);
}

bool starts_with(const std::string& s, std::size_t pos, const char* prefix) {
    const std::string p = prefix;
    return s.compare(pos, p.size(), p) == 0;
}

} // namespace

bool parse_junit(const std::string& xml, TestTally& out) {
    TestTally t;
    t.source = "junit";
    std::size_t pos = 0;
    while ((pos = xml.find('<', pos)) != std::string::npos) {
        if (!tag_at(xml, pos, "testcase")) {
            ++pos;
            continue;
        }
        const std::size_t tag_end = xml.find('>', pos);
        if (tag_end == std::string::npos) break;
        const std::string tag = xml.substr(pos, tag_end - pos + 1);
        const bool self_closing = tag_end > 0 && xml[tag_end - 1] == '/';
        std::string body;
        std::size_t next = tag_end + 1;
        if (!self_closing) {
            const std::size_t close = xml.find("</testcase>", tag_end);
            const std::size_t stop  = close == std::string::npos ? xml.size() : close;
            body = xml.substr(tag_end + 1, stop - tag_end - 1);
            next = stop;
        }
        const std::string status = attribute(tag, "status");
        bool failed = false, skipped = false;
        for (std::size_t b = body.find('<'); b != std::string::npos; b = body.find('<', b + 1)) {
            if (tag_at(body, b, "failure") || tag_at(body, b, "error")) failed = true;
            if (tag_at(body, b, "skipped")) skipped = true;
        }
        if (status == "fail") failed = true;
        if (status == "notrun" || status == "disabled") skipped = true;
        ++t.total;
        if (failed) {
            ++t.failed;
        } else if (skipped) {
            ++t.skipped;
        } else {
            ++t.passed;
        }
        pos = next;
    }
    if (t.total == 0) return false;
    out = t;
    return true;
}

bool parse_last_test_log(const std::string& log, TestTally& out) {
    TestTally t;
    t.source = "last-test-log";
    std::size_t line = 0;
    while (line < log.size()) {
        std::size_t end = log.find('\n', line);
        if (end == std::string::npos) end = log.size();
        if (starts_with(log, line, "Test Passed.")) {
            ++t.passed;
        } else if (starts_with(log, line, "Test Failed")) {
            ++t.failed;
        } else if (starts_with(log, line, "Test Not Run")) {
            ++t.skipped;
        }
        line = end + 1;
    }
    t.total = t.passed + t.failed + t.skipped;
    if (t.total == 0) return false;
    out = t;
    return true;
}

} // namespace ergo::package_manager
