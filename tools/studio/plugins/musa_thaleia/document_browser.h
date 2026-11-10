#pragma once
/// Markdown documents under one root (port of Ars-Musa thaleia
/// services/document_browser.rs). Differences from the Rust original:
///   - every path is resolved inside the root (the original read any path);
///   - search and the script map skip dot entries, like the tree does.
#include "markdown.h"

#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::plugins::musa_thaleia {

struct FileTreeNode {
    std::string               name;
    std::string               path;          ///< absolute, UTF-8
    bool                      is_directory = false;
    std::vector<FileTreeNode> children;      ///< directories first, then by lower-cased name
};
JsonValue to_json_value(const FileTreeNode& n);

struct Document {
    std::string                path;
    std::string                title;      ///< first "# " line, else the file stem
    std::string                content;
    std::optional<std::string> category;   ///< first directory under the root
};
JsonValue to_json_value(const Document& d);

struct RenderedDocument {
    std::string                path;
    std::string                title;      ///< first H1, else the file stem
    std::vector<Block>         blocks;
    std::vector<TocEntry>      toc;
    std::optional<std::string> category;
};
JsonValue        to_json_value(const RenderedDocument& d);
RenderedDocument render(const std::string& markdown, const std::string& path, const std::optional<std::string>& category);

class DocumentBrowser {
public:
    explicit DocumentBrowser(std::filesystem::path root);

    const std::filesystem::path& root() const { return root_; }

    /// Directories without any .md below them are left out.
    FileTreeNode               file_tree() const;
    /// `path` is absolute or relative to the root and must stay inside it.
    std::optional<Document>    read(const std::string& path) const;
    std::vector<std::string>   categories() const;
    /// Case-insensitive (ASCII) match on title or content.
    std::vector<Document>      search(const std::string& query) const;
    /// Document whose file stem equals `script_name` (map rebuilt by rescan()).
    std::optional<std::string> doc_for_script(const std::string& script_name) const;
    void                       rescan();

private:
    std::filesystem::path              root_;
    mutable std::mutex                 mtx_;
    std::map<std::string, std::string> script_to_doc_;
};

} // namespace ergo::studio::plugins::musa_thaleia
