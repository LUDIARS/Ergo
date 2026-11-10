#include "markdown.h"

#include <cctype>

namespace ergo::studio::plugins::musa_thaleia {
namespace {

std::vector<std::string> split_lines(const std::string& md) {
    std::vector<std::string> lines;
    std::string cur;
    for (char c : md) {
        if (c == '\n') {
            if (!cur.empty() && cur.back() == '\r') cur.pop_back();
            lines.push_back(std::move(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty() && cur.back() == '\r') cur.pop_back();
    if (!cur.empty()) lines.push_back(std::move(cur));
    return lines;
}

/// Columns of leading white space (a tab counts 4).
std::size_t indent_of(const std::string& l) {
    std::size_t n = 0;
    for (char c : l) {
        if (c == ' ') ++n;
        else if (c == '\t') n += 4;
        else break;
    }
    return n;
}

std::string ltrim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t");
    return b == std::string::npos ? std::string() : s.substr(b);
}

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(" \t") - b + 1);
}

bool blank(const std::string& l) { return l.find_first_not_of(" \t") == std::string::npos; }

bool is_rule(const std::string& l) {
    if (indent_of(l) > 3) return false;
    const std::string t = trim(l);
    if (t.empty() || (t[0] != '-' && t[0] != '*' && t[0] != '_')) return false;
    std::size_t n = 0;
    for (char c : t) {
        if (c == t[0]) ++n;
        else if (c != ' ' && c != '\t') return false;
    }
    return n >= 3;
}

/// Fence opener: (char, run length, info) when the line opens a fenced block.
bool fence_open(const std::string& l, char& ch, std::size_t& len, std::string& info) {
    if (indent_of(l) > 3) return false;
    const std::string t = ltrim(l);
    if (t.size() < 3 || (t[0] != '`' && t[0] != '~')) return false;
    std::size_t n = 0;
    while (n < t.size() && t[n] == t[0]) ++n;
    if (n < 3) return false;
    info = trim(t.substr(n));
    if (t[0] == '`' && info.find('`') != std::string::npos) return false;
    ch  = t[0];
    len = n;
    return true;
}

bool heading(const std::string& l, int& level, std::string& text) {
    if (indent_of(l) > 3) return false;
    const std::string t = ltrim(l);
    std::size_t n = 0;
    while (n < t.size() && t[n] == '#') ++n;
    if (n == 0 || n > 6 || (n < t.size() && t[n] != ' ' && t[n] != '\t')) return false;
    std::string body = trim(t.substr(n));
    // Optional closing sequence: " ###".
    const auto last = body.find_last_not_of('#');
    if (last == std::string::npos) body.clear();
    else if (last + 1 < body.size() && (body[last] == ' ' || body[last] == '\t')) body = trim(body.substr(0, last));
    level = static_cast<int>(n);
    text  = body;
    return true;
}

struct ListMarker {
    std::size_t indent  = 0;
    bool        ordered = false;
    int         number  = 0;
    std::string rest;
};

bool list_marker(const std::string& l, ListMarker& m) {
    m.indent = indent_of(l);
    const std::string t = ltrim(l);
    if (t.size() >= 2 && (t[0] == '-' || t[0] == '*' || t[0] == '+') && (t[1] == ' ' || t[1] == '\t')) {
        m.ordered = false;
        m.rest    = ltrim(t.substr(2));
        return true;
    }
    if (t.size() == 1 && (t[0] == '-' || t[0] == '*' || t[0] == '+')) {
        m.ordered = false;
        m.rest.clear();
        return true;
    }
    std::size_t d = 0;
    while (d < t.size() && d < 9 && std::isdigit(static_cast<unsigned char>(t[d]))) ++d;
    if (d == 0 || d >= t.size() || (t[d] != '.' && t[d] != ')')) return false;
    if (d + 1 < t.size() && t[d + 1] != ' ' && t[d + 1] != '\t') return false;
    m.ordered = true;
    m.number  = std::stoi(t.substr(0, d));
    m.rest    = d + 1 < t.size() ? ltrim(t.substr(d + 1)) : std::string();
    return true;
}

std::vector<std::string> split_row(const std::string& line) {
    std::string t = trim(line);
    if (!t.empty() && t.front() == '|') t.erase(0, 1);
    if (!t.empty() && t.back() == '|' && (t.size() < 2 || t[t.size() - 2] != '\\')) t.pop_back();
    std::vector<std::string> cells;
    std::string cur;
    for (std::size_t i = 0; i < t.size(); ++i) {
        if (t[i] == '\\' && i + 1 < t.size() && t[i + 1] == '|') {
            cur.push_back('|');
            ++i;
        } else if (t[i] == '|') {
            cells.push_back(trim(cur));
            cur.clear();
        } else {
            cur.push_back(t[i]);
        }
    }
    cells.push_back(trim(cur));
    return cells;
}

bool delimiter_row(const std::string& line, std::vector<Align>& aligns) {
    if (line.find('-') == std::string::npos) return false;
    aligns.clear();
    for (const auto& c : split_row(line)) {
        if (c.empty()) return false;
        const bool l = c.front() == ':';
        const bool r = c.back() == ':';
        const std::string dashes = c.substr(l ? 1 : 0, c.size() - (l ? 1 : 0) - (r ? 1 : 0));
        if (dashes.empty() || dashes.find_first_not_of('-') != std::string::npos) return false;
        aligns.push_back(l && r ? Align::Center : r ? Align::Right : l ? Align::Left : Align::None);
    }
    return true;
}

class BlockParser {
public:
    explicit BlockParser(std::vector<std::string> lines) : lines_(std::move(lines)) {}

    std::vector<Block> run() {
        while (i_ < lines_.size()) step();
        end_paragraph();
        return std::move(out_);
    }

private:
    void step() {
        const std::string& l = lines_[i_];
        if (blank(l)) {
            end_paragraph();
            ++i_;
            return;
        }
        char ch; std::size_t len; std::string info;
        if (fence_open(l, ch, len, info)) return fenced(ch, len, info);
        int level; std::string text;
        if (heading(l, level, text)) {
            end_paragraph();
            push_heading(level, text);
            ++i_;
            return;
        }
        // Setext heading: a paragraph underlined with === or ---.
        if (!para_.empty() && indent_of(l) <= 3) {
            const std::string t = trim(l);
            if (!t.empty() && t.find_first_not_of(t[0]) == std::string::npos && (t[0] == '=' || t[0] == '-')) {
                const std::string body = para_;
                para_.clear();
                push_heading(t[0] == '=' ? 1 : 2, body);
                ++i_;
                return;
            }
        }
        if (is_rule(l)) {
            end_paragraph();
            Block b;
            b.kind = BlockKind::Rule;
            out_.push_back(std::move(b));
            ++i_;
            return;
        }
        if (indent_of(l) <= 3 && ltrim(l)[0] == '>') return quote();
        ListMarker m;
        if (list_marker(l, m) && (para_.empty() || m.indent == 0)) return list_item(m);
        std::vector<Align> aligns;
        if (para_.empty() && l.find('|') != std::string::npos && i_ + 1 < lines_.size() &&
            delimiter_row(lines_[i_ + 1], aligns) && split_row(l).size() == aligns.size()) {
            return table(aligns);
        }
        if (!para_.empty()) para_.push_back('\n');
        para_ += ltrim(l);
        ++i_;
    }

    void end_paragraph() {
        if (para_.empty()) return;
        // Trailing hard-break markers at the very end mean nothing.
        while (!para_.empty() && (para_.back() == ' ' || para_.back() == '\\')) para_.pop_back();
        Block b;
        b.kind    = BlockKind::Paragraph;
        b.inlines = parse_inlines(para_);
        out_.push_back(std::move(b));
        para_.clear();
    }

    void push_heading(int level, const std::string& text) {
        Block b;
        b.kind    = BlockKind::Heading;
        b.level   = level;
        b.inlines = parse_inlines(text);
        b.anchor  = slugify(plain_text(b.inlines));
        out_.push_back(std::move(b));
    }

    void fenced(char ch, std::size_t len, const std::string& info) {
        end_paragraph();
        const std::size_t indent = indent_of(lines_[i_]);
        Block b;
        b.kind = BlockKind::Code;
        b.lang = info.substr(0, info.find(' '));
        ++i_;
        bool first = true;
        while (i_ < lines_.size()) {
            const std::string& l = lines_[i_];
            const std::string t  = ltrim(l);
            std::size_t n = 0;
            while (n < t.size() && t[n] == ch) ++n;
            if (indent_of(l) <= 3 && n >= len && trim(t.substr(n)).empty()) {
                ++i_;
                break;
            }
            // Remove up to the opener's indentation from each content line.
            std::size_t drop = 0;
            while (drop < indent && drop < l.size() && l[drop] == ' ') ++drop;
            if (!first) b.code.push_back('\n');
            b.code += l.substr(drop);
            first = false;
            ++i_;
        }
        out_.push_back(std::move(b));
    }

    void quote() {
        end_paragraph();
        std::vector<std::string> inner;
        while (i_ < lines_.size() && !blank(lines_[i_]) && indent_of(lines_[i_]) <= 3 && ltrim(lines_[i_])[0] == '>') {
            std::string t = ltrim(lines_[i_]).substr(1);
            if (!t.empty() && t[0] == ' ') t.erase(0, 1);
            inner.push_back(std::move(t));
            ++i_;
        }
        Block b;
        b.kind     = BlockKind::Quote;
        b.children = BlockParser(std::move(inner)).run();
        out_.push_back(std::move(b));
    }

    void list_item(const ListMarker& m) {
        end_paragraph();
        Block b;
        b.kind    = BlockKind::ListItem;
        b.level   = static_cast<int>(m.indent / 2 > 8 ? 8 : m.indent / 2);
        b.ordered = m.ordered;
        b.number  = m.number;
        std::string text = m.rest;
        if (text.size() >= 3 && text[0] == '[' && text[2] == ']' && (text.size() == 3 || text[3] == ' ')) {
            const char mark = text[1];
            if (mark == ' ' || mark == 'x' || mark == 'X') {
                b.checked = mark != ' ';
                text      = ltrim(text.substr(3));
            }
        }
        ++i_;
        // Lazy continuation: following lines that start no other block belong to the item.
        while (i_ < lines_.size()) {
            const std::string& l = lines_[i_];
            ListMarker next;
            char ch; std::size_t len; std::string info; int lv; std::string ht;
            if (blank(l) || list_marker(l, next) || fence_open(l, ch, len, info) || heading(l, lv, ht) || is_rule(l) ||
                ltrim(l)[0] == '>') {
                break;
            }
            text += "\n" + ltrim(l);
            ++i_;
        }
        b.inlines = parse_inlines(text);
        out_.push_back(std::move(b));
    }

    void table(const std::vector<Align>& aligns) {
        Block b;
        b.kind   = BlockKind::Table;
        b.aligns = aligns;
        auto add_row = [&](const std::string& line) {
            auto cells = split_row(line);
            cells.resize(aligns.size());   // GFM: pad short rows, drop extra cells
            std::vector<std::vector<Inline>> row;
            for (const auto& c : cells) row.push_back(parse_inlines(c));
            b.rows.push_back(std::move(row));
        };
        add_row(lines_[i_]);
        i_ += 2;
        while (i_ < lines_.size() && !blank(lines_[i_]) && lines_[i_].find('|') != std::string::npos) {
            add_row(lines_[i_]);
            ++i_;
        }
        out_.push_back(std::move(b));
    }

    std::vector<std::string> lines_;
    std::size_t              i_ = 0;
    std::string              para_;
    std::vector<Block>       out_;
};

void collect_toc(const std::vector<Block>& blocks, std::vector<TocEntry>& out) {
    for (const auto& b : blocks) {
        if (b.kind == BlockKind::Heading) out.push_back({b.level, plain_text(b.inlines), b.anchor});
        else if (b.kind == BlockKind::Quote) collect_toc(b.children, out);
    }
}

} // namespace

std::vector<Block> parse_markdown(const std::string& md) { return BlockParser(split_lines(md)).run(); }

std::vector<TocEntry> extract_toc(const std::vector<Block>& blocks) {
    std::vector<TocEntry> out;
    collect_toc(blocks, out);
    return out;
}

std::optional<std::string> first_h1(const std::vector<Block>& blocks) {
    for (const auto& t : extract_toc(blocks)) {
        if (t.level == 1 && !t.title.empty()) return t.title;
    }
    return std::nullopt;
}

} // namespace ergo::studio::plugins::musa_thaleia
