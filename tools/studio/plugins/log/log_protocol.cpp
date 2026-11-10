#include "log_protocol.h"

#include <cmath>

namespace ergo::studio::plugins::log {
namespace {

std::optional<double> finite_number(const JsonValue* v) {
    if (!v || !v->is_number() || !std::isfinite(v->n)) return std::nullopt;
    return v->n;
}

bool is_continuation(unsigned char c) { return (c & 0xC0) == 0x80; }

} // namespace

bool is_level_tag(const std::string& s) { return s == "ERR" || s == "WRN" || s == "INF" || s == "DBG"; }

JsonValue to_json_value(const StoredLine& l) {
    return jobj({{"lvl", jstr(l.lvl)}, {"frame", jnum(l.frame)}, {"ts_us", jnum(l.ts_us)},
                 {"tid", jnum(l.tid)}, {"msg", jstr(l.msg)}, {"seq", jnum(static_cast<double>(l.seq))}});
}

std::string clip_message(const std::string& msg) {
    if (msg.size() <= kMaxMsgBytes) return msg;
    std::size_t cut = kMaxMsgBytes;
    // Step back to the lead byte so a multi-byte character is never split.
    while (cut > 0 && is_continuation(static_cast<unsigned char>(msg[cut]))) --cut;
    return msg.substr(0, cut) + "\xE2\x80\xA6";
}

std::optional<LogLine> sanitize_line(const JsonValue& raw) {
    if (!raw.is_object()) return std::nullopt;
    const JsonValue* lvl = raw.find("lvl");
    const JsonValue* msg = raw.find("msg");
    if (!lvl || !lvl->is_string() || !is_level_tag(lvl->s) || !msg || !msg->is_string()) return std::nullopt;
    const auto frame = finite_number(raw.find("frame"));
    const auto ts_us = finite_number(raw.find("ts_us"));
    const auto tid   = finite_number(raw.find("tid"));
    if (!frame || !ts_us || !tid) return std::nullopt;
    return LogLine{lvl->s, *frame, *ts_us, *tid, clip_message(msg->s)};
}

std::optional<LinesBody> sanitize_lines_msg(const JsonValue& msg) {
    const JsonValue* lines = field(msg, "lines");
    if (!lines || !lines->is_array()) return std::nullopt;
    LinesBody body;
    for (const auto& l : *lines->a) {
        if (auto line = sanitize_line(l)) body.lines.push_back(std::move(*line));
    }
    const JsonValue* app = field(msg, "app");
    if (app && app->is_string()) body.app = app->s;
    const auto dropped = finite_number(field(msg, "dropped"));
    body.dropped       = dropped && *dropped > 0 ? static_cast<uint64_t>(std::floor(*dropped)) : 0;
    return body;
}

std::optional<ConsoleLine> parse_console_line(const std::string& text) {
    std::string t = text;
    if (!t.empty() && t.back() == '\n') t.pop_back();
    if (!t.empty() && t.back() == '\r') t.pop_back();
    // [F<digits>][TAG] msg   or   [F<digits>][TAG]
    if (t.size() < 3 || t[0] != '[' || t[1] != 'F') return std::nullopt;
    std::size_t i = 2;
    while (i < t.size() && t[i] >= '0' && t[i] <= '9') ++i;
    if (i == 2 || i + 6 > t.size() || t[i] != ']' || t[i + 1] != '[' || t[i + 5] != ']') return std::nullopt;
    const std::string tag = t.substr(i + 2, 3);
    if (!is_level_tag(tag)) return std::nullopt;
    const std::string rest = t.substr(i + 6);
    if (!rest.empty() && rest[0] != ' ') return std::nullopt;
    ConsoleLine c;
    c.lvl   = tag;
    c.frame = std::stoull(t.substr(2, i - 2));
    c.msg   = rest.empty() ? std::string() : rest.substr(1);
    return c;
}

} // namespace ergo::studio::plugins::log
