#include "ergo/package_manager/leak_check.h"

#include <algorithm>
#include <cctype>

namespace ergo::package_manager {

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

/// JSON Pointer の 1 段 (RFC 6901 のエスケープ)。
std::string pointer_token(const std::string& key) {
    std::string out;
    for (const char c : key) {
        if (c == '~') out += "~0";
        else if (c == '/') out += "~1";
        else out += c;
    }
    return out;
}

void walk(const jsonm::JsonValue& v, const std::string& where, const std::string& key,
          std::vector<Leak>& out) {
    if (v.is_string()) {
        if (looks_like_local_path(v.s)) out.push_back({where, "absolute path of a machine"});
        if (!key.empty() && is_secret_key(key)) out.push_back({where, "credential-like key"});
        return;
    }
    if (!key.empty() && is_secret_key(key) && !v.is_null()) {
        out.push_back({where, "credential-like key"});
    }
    if (v.is_array() && v.a) {
        for (std::size_t i = 0; i < v.a->size(); ++i) {
            walk((*v.a)[i], where + "/" + std::to_string(i), {}, out);
        }
    } else if (v.is_object() && v.o) {
        for (const auto& [k, child] : *v.o) {
            const std::string at = where + "/" + pointer_token(k);
            if (looks_like_local_path(k)) out.push_back({at, "absolute path of a machine (key)"});
            walk(child, at, k, out);
        }
    }
}

} // namespace

bool looks_like_local_path(const std::string& value) {
    if (value.size() >= 3 && std::isalpha(static_cast<unsigned char>(value[0])) && value[1] == ':' &&
        (value[2] == '/' || value[2] == '\\')) {
        return true;
    }
    // UNC (\\host\share, also written with escaped backslashes): a host segment then a separator.
    // A regex back-reference such as "\\1" has no separator after it and is not a path.
    if (value.rfind("\\\\", 0) == 0) {
        const std::size_t host = value.find_first_not_of('\\');
        if (host != std::string::npos) {
            const std::size_t sep = value.find_first_of("\\/", host);
            if (sep != std::string::npos && sep > host) return true;
        }
    }
    return value.find("/Users/") != std::string::npos || value.find("/home/") != std::string::npos;
}

bool is_secret_key(const std::string& key) {
    const std::string k = lower(key);
    return k.find("token") != std::string::npos || k.find("secret") != std::string::npos ||
           k.find("authorization") != std::string::npos;
}

std::vector<Leak> find_leaks(const jsonm::JsonValue& doc) {
    std::vector<Leak> out;
    walk(doc, "", {}, out);
    return out;
}

std::vector<Leak> find_text_leaks(const std::string& text) {
    std::vector<Leak> out;
    std::size_t line_no = 1;
    std::string word;
    auto flush = [&] {
        if (!word.empty() && looks_like_local_path(word)) {
            out.push_back({"line " + std::to_string(line_no), "absolute path of a machine"});
        }
        word.clear();
    };
    for (const char c : text) {
        if (c == '\n') {
            flush();
            ++line_no;
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '"' || c == '\'' || c == '(' ||
                   c == ')') {
            flush();
        } else {
            word += c;
        }
    }
    flush();
    return out;
}

std::vector<Finding> leak_findings(const std::vector<Leak>& leaks, const std::string& subject) {
    std::vector<Finding> out;
    for (const auto& l : leaks) {
        const std::string at = l.where.empty() ? std::string("/") : l.where;
        out.push_back({"LEAK", Severity::Error, subject, l.reason + " at " + at});
    }
    return out;
}

} // namespace ergo::package_manager
