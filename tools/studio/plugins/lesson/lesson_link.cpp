#include "lesson_link.h"

namespace ergo::studio::plugins::lesson {
namespace {

const char* const kPath          = "/v1/tool";
const char* const kCodeAlphabet  = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";   // Crockford base32
constexpr std::size_t kCodeLength = 10;

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
bool is_digit(char c) { return c >= '0' && c <= '9'; }
bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool is_hex(char c) { return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }
char upper(char c) { return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c; }

std::string lowered(std::string s) {
    for (char& c : s) c = lower(c);
    return s;
}

std::string trimmed(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && is_space(s[b])) ++b;
    while (e > b && is_space(s[e - 1])) --e;
    return s.substr(b, e - b);
}

LinkParse reject(const char* why) { return {std::nullopt, why}; }

/// "example.org" / "127.0.0.1": letters, digits, '.', '-'; no empty label.
bool is_reg_name(const std::string& h) {
    if (h.empty() || h.front() == '.' || h.back() == '.' || h.front() == '-' || h.back() == '-') return false;
    char prev = '.';
    for (char c : h) {
        if (!(is_alpha(c) || is_digit(c) || c == '.' || c == '-')) return false;
        if (c == '.' && prev == '.') return false;
        prev = c;
    }
    return true;
}

/// The inside of "[::1]": hex digits, ':' and '.' with at least one ':'.
bool is_ipv6_body(const std::string& h) {
    bool colon = false;
    for (char c : h) {
        if (c == ':') colon = true;
        else if (!is_hex(c) && c != '.') return false;
    }
    return colon;
}

/// 1..65535 without a leading zero.
bool is_port(const std::string& p) {
    if (p.empty() || p.size() > 5 || p.front() == '0') return false;
    unsigned v = 0;
    for (char c : p) {
        if (!is_digit(c)) return false;
        v = v * 10 + static_cast<unsigned>(c - '0');
    }
    return v >= 1 && v <= 65535;
}

} // namespace

LinkParse parse_lesson_link(const std::string& text) {
    if (text.size() > kMaxLinkText) return reject("接続文字列が長すぎます (2048 バイトまで)");
    const std::string s = trimmed(text);
    if (s.empty()) return reject("接続文字列が空です");
    for (char c : s) {
        const auto u = static_cast<unsigned char>(c);
        if (u <= 0x20 || u == 0x7F) return reject("接続文字列に空白や改行が入っています");
        if (u >= 0x80) return reject("接続文字列に使えない文字 (全角文字など) が入っています");
    }

    const auto scheme_end = s.find("://");
    if (scheme_end == std::string::npos) return reject("http:// か https:// で始まる接続文字列ではありません");
    const std::string scheme = lowered(s.substr(0, scheme_end));
    if (scheme != "http" && scheme != "https") return reject("http:// か https:// で始まる接続文字列ではありません");

    const std::string rest = s.substr(scheme_end + 3);
    const auto hash = rest.find('#');
    if (hash == std::string::npos) return reject("コード (# の後ろ) がありません");
    const std::string before = rest.substr(0, hash);
    const std::string code_in = rest.substr(hash + 1);

    if (before.find('?') != std::string::npos) return reject("クエリ (?) の付いた接続文字列は使えません");
    const auto slash = before.find('/');
    const std::string authority = before.substr(0, slash);
    const std::string path      = slash == std::string::npos ? std::string() : before.substr(slash);
    if (authority.find('@') != std::string::npos) return reject("ユーザ情報 (@) の付いた接続文字列は使えません");
    if (authority.empty()) return reject("ホスト名がありません");

    std::string host, port;
    bool        bracketed = false;
    if (authority.front() == '[') {
        const auto close = authority.find(']');
        if (close == std::string::npos) return reject("ホスト名が正しくありません");
        host      = authority.substr(1, close - 1);
        bracketed = true;
        const std::string after = authority.substr(close + 1);
        if (!after.empty()) {
            if (after.front() != ':') return reject("ホスト名が正しくありません");
            port = after.substr(1);
            if (!is_port(port)) return reject("ポート番号が正しくありません");
        }
        if (!is_ipv6_body(host)) return reject("ホスト名が正しくありません");
    } else {
        const auto colon = authority.find(':');
        host = authority.substr(0, colon);
        if (colon != std::string::npos) {
            port = authority.substr(colon + 1);
            if (!is_port(port)) return reject("ポート番号が正しくありません");
        }
        if (!is_reg_name(host)) return reject("ホスト名が正しくありません");
    }

    if (path != kPath) return reject("パスが /v1/tool ではありません");

    if (code_in.size() != kCodeLength) return reject("コードは 10 文字です");
    std::string code;
    for (char c : code_in) {
        const char u = upper(c);
        if (std::string(kCodeAlphabet).find(u) == std::string::npos) {
            return reject("コードに使えない文字が入っています (使えるのは 0-9 と、I L O U を除く A-Z)");
        }
        code.push_back(u);
    }

    LessonLink link;
    link.host   = lowered(host);
    link.origin = scheme + "://" + (bracketed ? "[" + link.host + "]" : link.host) + (port.empty() ? "" : ":" + port);
    link.code   = std::move(code);
    return {std::move(link), {}};
}

} // namespace ergo::studio::plugins::lesson
