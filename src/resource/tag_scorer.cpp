#include "ergo/resource/tag_scorer.h"

#include <algorithm>
#include <unordered_set>

namespace ergo::resource {
namespace {

// Tags are UTF-8 and frequently non-ASCII (Japanese tag vocab shared with
// Curare / MUSA Clio). std::tolower/std::isspace take an int and their
// behavior for values outside 0-127 is locale-dependent even when the byte
// is first cast to unsigned char: under a non-"C" global locale they can
// remap individual bytes of a multibyte UTF-8 sequence, corrupting it and
// producing wrong (or just non-deterministic) matches. Fold/trim ASCII only
// and pass every byte >= 0x80 through unchanged instead.
//
// Known limitation: scripts that carry a non-ASCII case distinction (e.g.
// full-width Latin, Cyrillic) are compared byte-for-byte rather than
// case-folded, so "ＡＢＣ" vs "ａｂｃ" won't match. CJK tags (no case
// concept) already compare correctly since there's nothing to fold. Full
// Unicode case folding/normalization is out of scope for this module (see
// spec/module/resource.md); this only guarantees non-ASCII bytes can never
// crash or get mangled into a false match.
bool is_ascii_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
          c == '\f' || c == '\v';
}

char ascii_tolower(unsigned char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a')
                                  : static_cast<char>(c);
}

std::string normalize(const std::string& value) {
    std::string out;
    out.reserve(value.size());

    // trim
    std::size_t begin = 0;
    std::size_t end   = value.size();
    while (begin < end && is_ascii_space(static_cast<unsigned char>(value[begin]))) ++begin;
    while (end > begin && is_ascii_space(static_cast<unsigned char>(value[end - 1]))) --end;

    for (std::size_t i = begin; i < end; ++i) {
        out.push_back(ascii_tolower(static_cast<unsigned char>(value[i])));
    }
    return out;
}

std::vector<std::string> tokenize(const std::string& normalized) {
    std::vector<std::string> tokens;
    std::string current;
    auto flush = [&] {
        if (current.size() >= 2) tokens.push_back(current);
        current.clear();
    };
    for (char c : normalized) {
        if (c == ' ' || c == '_' || c == '-' || c == '.') {
            flush();
        } else {
            current.push_back(c);
        }
    }
    flush();
    return tokens;
}

} // namespace

int score(const std::vector<std::string>& candidate_tags,
          const std::vector<std::string>& wanted_tags,
          const std::string& candidate_name,
          const std::string& name_hint) {
    int total = 0;

    std::unordered_set<std::string> candidate_set;
    for (const auto& tag : candidate_tags) {
        auto normalized = normalize(tag);
        if (!normalized.empty()) candidate_set.insert(std::move(normalized));
    }
    for (const auto& tag : wanted_tags) {
        auto normalized = normalize(tag);
        if (!normalized.empty() && candidate_set.count(normalized) > 0) {
            total += kTagMatchScore;
        }
    }

    const std::string name = normalize(candidate_name);
    const std::string hint = normalize(name_hint);
    if (!name.empty() && !hint.empty()) {
        if (name == hint) {
            total += kNameExactScore;
        } else {
            for (const auto& token : tokenize(hint)) {
                if (name.find(token) != std::string::npos) {
                    total += kNameTokenScore;
                }
            }
        }
    }

    return total;
}

std::string extract_name_hint(const std::string& object_name) {
    std::string trimmed = object_name;

    // trim outer whitespace (ASCII-only trim — see comment above normalize()
    // for why std::isspace isn't used directly on raw bytes here)
    while (!trimmed.empty() &&
           is_ascii_space(static_cast<unsigned char>(trimmed.front()))) {
        trimmed.erase(trimmed.begin());
    }
    while (!trimmed.empty() &&
           is_ascii_space(static_cast<unsigned char>(trimmed.back()))) {
        trimmed.pop_back();
    }

    // strip a trailing "(N)" duplicate counter
    if (!trimmed.empty() && trimmed.back() == ')') {
        const auto open = trimmed.rfind('(');
        if (open != std::string::npos && open + 1 < trimmed.size() - 1) {
            bool digits = true;
            for (std::size_t i = open + 1; i < trimmed.size() - 1; ++i) {
                const unsigned char ch = static_cast<unsigned char>(trimmed[i]);
                if (ch < '0' || ch > '9') {
                    digits = false;
                    break;
                }
            }
            if (digits) {
                trimmed.erase(open);
                while (!trimmed.empty() &&
                       is_ascii_space(static_cast<unsigned char>(trimmed.back()))) {
                    trimmed.pop_back();
                }
            }
        }
    }

    if (normalize(trimmed) == "cube") return "";
    return trimmed;
}

} // namespace ergo::resource
