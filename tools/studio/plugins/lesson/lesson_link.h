#pragma once
/// The connection string a student copies from the odeum-lesson page:
///   <origin>/v1/tool#<CODE>
/// origin = http(s)://host[:port] (no userinfo, no query), CODE = 10 Crockford
/// base32 symbols. Pure parsing; nothing is sent anywhere.
#include <optional>
#include <string>

namespace ergo::studio::plugins::lesson {

struct LessonLink {
    std::string origin;   ///< "https://lesson.example.org" / "http://127.0.0.1:4410" (scheme and host lowercased)
    std::string host;     ///< host without the port: the only part the view shows
    std::string code;     ///< 10 symbols, uppercased. A secret: never logged or shown.
};

struct LinkParse {
    std::optional<LessonLink> link;
    std::string               error;   ///< why it was rejected (Japanese, shown to the user; never echoes the input)
};

/// Leading / trailing whitespace (a copied line break) is ignored; everything
/// else must match exactly.
LinkParse parse_lesson_link(const std::string& text);

} // namespace ergo::studio::plugins::lesson
