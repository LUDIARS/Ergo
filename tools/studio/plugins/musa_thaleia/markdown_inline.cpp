#include "markdown.h"

#include <cctype>

namespace ergo::studio::plugins::musa_thaleia {
namespace {

struct Style {
    bool strong = false;
    bool em     = false;
    bool strike = false;
};

bool is_punct(char c) { return std::ispunct(static_cast<unsigned char>(c)) != 0; }
bool is_word(char c) {
    const auto u = static_cast<unsigned char>(c);
    return std::isalnum(u) != 0 || u >= 0x80;
}

class InlineParser {
public:
    InlineParser(const std::string& s, Style st) : s_(s), st_(st) {}

    std::vector<Inline> run() {
        while (i_ < s_.size()) step();
        flush();
        return std::move(out_);
    }

private:
    void step() {
        const char c = s_[i_];
        if (c == '\\') return escape();
        if (c == '`') return code_span();
        if (c == '!' && peek(1) == '[') return link(true);
        if (c == '[') return link(false);
        if (c == '<') return autolink();
        if ((c == '*' || c == '_') && peek(1) == c) return emphasis(std::string(2, c), true, false);
        if (c == '~' && peek(1) == '~') return emphasis("~~", false, true);
        if (c == '*' || c == '_') return emphasis(std::string(1, c), false, false);
        if (c == '\n') return newline();
        buf_.push_back(c);
        ++i_;
    }

    char peek(std::size_t k) const { return i_ + k < s_.size() ? s_[i_ + k] : '\0'; }

    void flush() {
        if (buf_.empty()) return;
        Inline in;
        in.text   = std::move(buf_);
        in.strong = st_.strong;
        in.em     = st_.em;
        in.strike = st_.strike;
        out_.push_back(std::move(in));
        buf_.clear();
    }

    void push(Inline in) {
        flush();
        in.strong = in.strong || st_.strong;
        in.em     = in.em || st_.em;
        in.strike = in.strike || st_.strike;
        out_.push_back(std::move(in));
    }

    void escape() {
        const char n = peek(1);
        if (n == '\n') {  // backslash hard break
            push({InlineKind::Break, {}, {}});
            i_ += 2;
        } else if (n != '\0' && is_punct(n)) {
            buf_.push_back(n);
            i_ += 2;
        } else {
            buf_.push_back('\\');
            ++i_;
        }
    }

    void newline() {
        // Two trailing spaces make a hard break; otherwise a soft break is a space.
        std::size_t spaces = 0;
        while (!buf_.empty() && buf_.back() == ' ') {
            buf_.pop_back();
            ++spaces;
        }
        if (spaces >= 2) push({InlineKind::Break, {}, {}});
        else buf_.push_back(' ');
        ++i_;
    }

    void code_span() {
        std::size_t n = 0;
        while (peek(n) == '`') ++n;
        const std::string fence(n, '`');
        std::size_t search = i_ + n;
        while (true) {
            const auto close = s_.find(fence, search);
            if (close == std::string::npos) {
                buf_.append(fence);
                i_ += n;
                return;
            }
            // The closing run must be exactly n long.
            std::size_t run = 0;
            while (close + run < s_.size() && s_[close + run] == '`') ++run;
            if (run != n) {
                search = close + run;
                continue;
            }
            std::string body = s_.substr(i_ + n, close - i_ - n);
            for (char& ch : body) {
                if (ch == '\n') ch = ' ';
            }
            if (body.size() >= 2 && body.front() == ' ' && body.back() == ' ' &&
                body.find_first_not_of(' ') != std::string::npos) {
                body = body.substr(1, body.size() - 2);
            }
            Inline in{InlineKind::Code, std::move(body), {}};
            push(std::move(in));
            i_ = close + n;
            return;
        }
    }

    /// [label](url) / ![alt](url). Falls back to literal text when incomplete.
    void link(bool image) {
        const std::size_t open = i_ + (image ? 1 : 0);
        int depth = 0;
        std::size_t close = std::string::npos;
        for (std::size_t j = open; j < s_.size(); ++j) {
            if (s_[j] == '\\') { ++j; continue; }
            if (s_[j] == '[') ++depth;
            else if (s_[j] == ']' && --depth == 0) { close = j; break; }
        }
        if (close == std::string::npos || close + 1 >= s_.size() || s_[close + 1] != '(') return literal(image ? 2 : 1);
        const auto end = s_.find(')', close + 2);
        if (end == std::string::npos) return literal(image ? 2 : 1);
        std::string url = s_.substr(close + 2, end - close - 2);
        // Drop an optional "title" after the destination.
        if (const auto sp = url.find(' '); sp != std::string::npos) url = url.substr(0, sp);
        if (url.size() >= 2 && url.front() == '<' && url.back() == '>') url = url.substr(1, url.size() - 2);
        const std::string label = s_.substr(open + 1, close - open - 1);
        Inline in;
        in.kind = image ? InlineKind::Image : InlineKind::Link;
        in.text = plain_text(InlineParser(label, st_).run());
        in.url  = std::move(url);
        push(std::move(in));
        i_ = end + 1;
    }

    void autolink() {
        const auto end = s_.find('>', i_ + 1);
        if (end != std::string::npos) {
            const std::string inner = s_.substr(i_ + 1, end - i_ - 1);
            const bool url = inner.rfind("http://", 0) == 0 || inner.rfind("https://", 0) == 0 ||
                             inner.rfind("mailto:", 0) == 0;
            if (url && inner.find(' ') == std::string::npos) {
                push({InlineKind::Link, inner, inner});
                i_ = end + 1;
                return;
            }
        }
        literal(1);
    }

    void emphasis(const std::string& d, bool strong, bool strike) {
        const std::size_t start = i_ + d.size();
        // An opener must be followed by non-space; '_' must not sit inside a word.
        const bool left_ok = start < s_.size() && s_[start] != ' ' && s_[start] != '\n';
        const bool intraword = d[0] == '_' && i_ > 0 && is_word(s_[i_ - 1]);
        if (!left_ok || intraword) return literal(d.size());
        std::size_t j = start;
        while (true) {
            auto close = s_.find(d, j);
            if (close == std::string::npos) return literal(d.size());
            // In a longer run ("***" closing "**"), the closer is the run's tail:
            // the head belongs to an inner single delimiter.
            if (d.size() == 2) {
                std::size_t run = 0;
                while (close + run < s_.size() && s_[close + run] == d[0]) ++run;
                if (run > 2) close += run - 2;
            }
            const bool escaped   = close > 0 && s_[close - 1] == '\\';
            const bool after_sp  = s_[close - 1] == ' ';
            // For a single delimiter, skip a double one (that belongs to strong).
            const bool doubled   = d.size() == 1 && close + 1 < s_.size() && s_[close + 1] == d[0];
            const bool intra_end = d[0] == '_' && close + d.size() < s_.size() && is_word(s_[close + d.size()]);
            if (close > start && !escaped && !after_sp && !doubled && !intra_end) {
                Style inner = st_;
                if (strike) inner.strike = true;
                else if (strong) inner.strong = true;
                else inner.em = true;
                flush();
                for (auto& in : InlineParser(s_.substr(start, close - start), inner).run()) out_.push_back(std::move(in));
                i_ = close + d.size();
                return;
            }
            j = close + (doubled ? 2 : 1);
        }
    }

    void literal(std::size_t n) {
        buf_.append(s_, i_, n);
        i_ += n;
    }

    const std::string&  s_;
    Style               st_;
    std::size_t         i_ = 0;
    std::string         buf_;
    std::vector<Inline> out_;
};

} // namespace

std::vector<Inline> parse_inlines(const std::string& text) { return InlineParser(text, {}).run(); }

std::string plain_text(const std::vector<Inline>& inlines) {
    std::string s;
    for (const auto& in : inlines) s += in.kind == InlineKind::Break ? std::string(" ") : in.text;
    return s;
}

std::string slugify(const std::string& text) {
    std::string stripped;
    bool in_tag = false;
    for (char c : text) {
        if (c == '<') in_tag = true;
        else if (c == '>' && in_tag) in_tag = false;
        else if (!in_tag) stripped.push_back(c);
    }
    std::string out;
    bool pending_dash = false;
    for (char c : stripped) {
        const auto u    = static_cast<unsigned char>(c);
        const bool keep = std::isalnum(u) != 0 || u >= 0x80 || c == '_';
        if (!keep) {  // '-' and every other character separate words
            pending_dash = !out.empty();
            continue;
        }
        if (pending_dash) out.push_back('-');
        pending_dash = false;
        out.push_back(static_cast<char>(std::tolower(u)));
    }
    return out;
}

} // namespace ergo::studio::plugins::musa_thaleia
