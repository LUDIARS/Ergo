#include "ergo/package_scan/json_lines.h"

#include <cstddef>

namespace ergo::package_scan {

namespace {

constexpr char kSep = '\x1f';

std::string join(const std::vector<std::string>& path) {
    std::string out;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i) out += kSep;
        out += path[i];
    }
    return out;
}

/// 1 段の入れ子: オブジェクトなら次のキー、 配列なら次の添字を待つ。
struct Frame {
    bool        array = false;
    std::size_t index = 0;
    std::string key;
    bool        have_key = false;
};

class Scanner {
public:
    explicit Scanner(const std::string& text) : text_(text) {}

    std::map<std::string, int> run() {
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c == '\n') {
                ++line_;
                ++pos_;
            } else if (c == '{' || c == '[') {
                begin_value();
                stack_.push_back(Frame{c == '[', 0, {}, false});
                ++pos_;
            } else if (c == '}' || c == ']') {
                if (!stack_.empty()) stack_.pop_back();
                ++pos_;
            } else if (c == '"') {
                std::string s = read_string();
                if (!stack_.empty() && !stack_.back().array && !stack_.back().have_key) {
                    stack_.back().key      = std::move(s);
                    stack_.back().have_key = true;
                } else {
                    begin_value();
                }
            } else if (c == ',') {
                if (!stack_.empty()) {
                    auto& f = stack_.back();
                    if (f.array) ++f.index;
                    f.have_key = false;
                }
                ++pos_;
            } else if (c == ':' || c == ' ' || c == '\t' || c == '\r') {
                ++pos_;
            } else {
                // 数値・true・false・null: 区切りまで読み飛ばす。
                begin_value();
                while (pos_ < text_.size() && text_[pos_] != ',' && text_[pos_] != '}' &&
                       text_[pos_] != ']' && text_[pos_] != '\n') {
                    ++pos_;
                }
            }
        }
        return std::move(lines_);
    }

private:
    void begin_value() {
        std::vector<std::string> path;
        for (const auto& f : stack_) path.push_back(f.array ? std::to_string(f.index) : f.key);
        lines_.emplace(join(path), line_);
    }

    std::string read_string() {
        std::string out;
        ++pos_;   // 開きの引用符
        while (pos_ < text_.size()) {
            const char c = text_[pos_++];
            if (c == '"') break;
            if (c == '\\' && pos_ < text_.size()) {
                const char e = text_[pos_++];
                out += e == 'n' ? '\n' : e == 't' ? '\t' : e;
                continue;
            }
            if (c == '\n') ++line_;
            out += c;
        }
        return out;
    }

    const std::string&         text_;
    std::size_t                pos_  = 0;
    int                        line_ = 1;
    std::vector<Frame>         stack_;
    std::map<std::string, int> lines_;
};

} // namespace

JsonLineIndex JsonLineIndex::build(const std::string& text) {
    JsonLineIndex index;
    index.lines_ = Scanner(text).run();
    return index;
}

int JsonLineIndex::line_of(const std::vector<std::string>& path) const {
    const auto it = lines_.find(join(path));
    return it == lines_.end() ? 0 : it->second;
}

} // namespace ergo::package_scan
