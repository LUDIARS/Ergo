#include "document_browser.h"

#include "../common/file_io.h"
#include "../common/safe_path.h"

#include <algorithm>
#include <functional>
#include <system_error>

namespace ergo::studio::plugins::musa_thaleia {
namespace fs = std::filesystem;
namespace {

std::string lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

bool hidden(const fs::path& p) {
    const std::string n = p.filename().u8string();
    return !n.empty() && n[0] == '.';
}

bool is_md(const fs::path& p) { return p.extension() == ".md"; }

void sort_tree(FileTreeNode& n) {
    std::sort(n.children.begin(), n.children.end(), [](const FileTreeNode& a, const FileTreeNode& b) {
        if (a.is_directory != b.is_directory) return a.is_directory;
        return lower(a.name) < lower(b.name);
    });
    for (auto& c : n.children) sort_tree(c);
}

void build_tree(const fs::path& dir, FileTreeNode& parent) {
    std::error_code ec;
    fs::directory_iterator it(dir, ec);
    if (ec) return;
    for (const auto& ent : it) {
        if (hidden(ent.path())) continue;
        std::error_code e2;
        if (ent.is_directory(e2)) {
            FileTreeNode node{ent.path().filename().u8string(), ent.path().u8string(), true, {}};
            build_tree(ent.path(), node);
            if (!node.children.empty()) parent.children.push_back(std::move(node));
        } else if (is_md(ent.path())) {
            parent.children.push_back({ent.path().filename().u8string(), ent.path().u8string(), false, {}});
        }
    }
}

/// Every .md under `root`, skipping dot entries.
void walk_md(const fs::path& root, const std::function<void(const fs::path&)>& fn) {
    std::error_code ec;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        if (hidden(it->path())) {
            if (it->is_directory(ec)) it.disable_recursion_pending();
            continue;
        }
        std::error_code e2;
        if (it->is_regular_file(e2) && is_md(it->path())) fn(it->path());
    }
}

JsonValue opt_str(const std::optional<std::string>& s) { return s ? jstr(*s) : JsonValue::make_null(); }

std::string stem_or_untitled(const std::string& path) {
    const std::string s = fs::u8path(path).stem().u8string();
    return s.empty() ? std::string("Untitled") : s;
}

} // namespace

JsonValue to_json_value(const FileTreeNode& n) {
    JsonValue kids = JsonValue::make_array();
    for (const auto& c : n.children) kids.push(to_json_value(c));
    return jobj({{"name", jstr(n.name)}, {"path", jstr(n.path)}, {"is_directory", jbool(n.is_directory)}, {"children", kids}});
}

JsonValue to_json_value(const Document& d) {
    return jobj({{"path", jstr(d.path)}, {"title", jstr(d.title)}, {"content", jstr(d.content)}, {"category", opt_str(d.category)}});
}

JsonValue to_json_value(const RenderedDocument& d) {
    return jobj({{"path", jstr(d.path)}, {"title", jstr(d.title)}, {"blocks", to_json_value(d.blocks)},
                 {"toc", to_json_value(d.toc)}, {"category", opt_str(d.category)}});
}

RenderedDocument render(const std::string& markdown, const std::string& path, const std::optional<std::string>& category) {
    RenderedDocument r;
    r.path     = path;
    r.blocks   = parse_markdown(markdown);
    r.toc      = extract_toc(r.blocks);
    r.title    = first_h1(r.blocks).value_or(stem_or_untitled(path));
    r.category = category;
    return r;
}

DocumentBrowser::DocumentBrowser(fs::path root) : root_(fs::absolute(root).lexically_normal()) { rescan(); }

FileTreeNode DocumentBrowser::file_tree() const {
    std::string name = root_.filename().u8string();
    if (name.empty()) name = "docs";
    FileTreeNode root{name, root_.u8string(), true, {}};
    build_tree(root_, root);
    sort_tree(root);
    return root;
}

std::optional<Document> DocumentBrowser::read(const std::string& path) const {
    const auto full = resolve_within(root_, path);
    if (!full) return std::nullopt;
    std::string err;
    auto content = read_text_file(*full, err);
    if (!content) return std::nullopt;

    Document d;
    d.path = full->u8string();
    std::size_t start = 0;
    while (start <= content->size()) {
        const auto end = content->find('\n', start);
        std::string line = content->substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("# ", 0) == 0) {
            d.title = line.substr(2);
            break;
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    if (d.title.empty()) d.title = stem_or_untitled(d.path);
    const fs::path rel = full->parent_path().lexically_relative(root_);
    if (!rel.empty() && rel != "." && rel.begin() != rel.end()) d.category = rel.begin()->u8string();
    d.content = std::move(*content);
    return d;
}

std::vector<std::string> DocumentBrowser::categories() const {
    std::vector<std::string> out;
    std::error_code ec;
    fs::directory_iterator it(root_, ec);
    if (ec) return out;
    for (const auto& ent : it) {
        std::error_code e2;
        if (ent.is_directory(e2) && !hidden(ent.path())) out.push_back(ent.path().filename().u8string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<Document> DocumentBrowser::search(const std::string& query) const {
    const std::string q = lower(query);
    std::vector<Document> out;
    walk_md(root_, [&](const fs::path& p) {
        auto d = read(p.u8string());
        if (d && (lower(d->title).find(q) != std::string::npos || lower(d->content).find(q) != std::string::npos)) {
            out.push_back(std::move(*d));
        }
    });
    std::sort(out.begin(), out.end(), [](const Document& a, const Document& b) { return a.path < b.path; });
    return out;
}

std::optional<std::string> DocumentBrowser::doc_for_script(const std::string& script_name) const {
    std::lock_guard<std::mutex> lk(mtx_);
    const auto it = script_to_doc_.find(script_name);
    if (it == script_to_doc_.end()) return std::nullopt;
    return it->second;
}

void DocumentBrowser::rescan() {
    std::map<std::string, std::string> m;
    walk_md(root_, [&](const fs::path& p) {
        const std::string stem = p.stem().u8string();
        // Deterministic on duplicates: the lexically first path wins.
        if (!stem.empty()) {
            auto [it, fresh] = m.emplace(stem, p.u8string());
            if (!fresh && p.u8string() < it->second) it->second = p.u8string();
        }
    });
    std::lock_guard<std::mutex> lk(mtx_);
    script_to_doc_ = std::move(m);
}

} // namespace ergo::studio::plugins::musa_thaleia
