#include "ergo/resource/tag_scorer.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace ergo::resource {
namespace {

std::string normalize(const std::string& value) {
    std::string out;
    out.reserve(value.size());

    // trim
    std::size_t begin = 0;
    std::size_t end   = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;

    for (std::size_t i = begin; i < end; ++i) {
        out.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(value[i]))));
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

    // trim outer whitespace
    while (!trimmed.empty() &&
           std::isspace(static_cast<unsigned char>(trimmed.front()))) {
        trimmed.erase(trimmed.begin());
    }
    while (!trimmed.empty() &&
           std::isspace(static_cast<unsigned char>(trimmed.back()))) {
        trimmed.pop_back();
    }

    // strip a trailing "(N)" duplicate counter
    if (!trimmed.empty() && trimmed.back() == ')') {
        const auto open = trimmed.rfind('(');
        if (open != std::string::npos && open + 1 < trimmed.size() - 1) {
            bool digits = true;
            for (std::size_t i = open + 1; i < trimmed.size() - 1; ++i) {
                if (!std::isdigit(static_cast<unsigned char>(trimmed[i]))) {
                    digits = false;
                    break;
                }
            }
            if (digits) {
                trimmed.erase(open);
                while (!trimmed.empty() &&
                       std::isspace(static_cast<unsigned char>(trimmed.back()))) {
                    trimmed.pop_back();
                }
            }
        }
    }

    if (normalize(trimmed) == "cube") return "";
    return trimmed;
}

} // namespace ergo::resource
