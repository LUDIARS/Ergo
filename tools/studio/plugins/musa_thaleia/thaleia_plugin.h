#pragma once
/// "musa_thaleia" plugin: Ars-Musa Thaleia (spec document viewer) inside the
/// Studio. Named musa_thaleia so it is not mistaken for LUDIARS/Thaleia (the
/// traceability service). HTTP API mirrors the Tauri commands:
///   GET  /api/tree                      get_file_tree
///   GET  /api/document?path=            read_document
///   GET  /api/render?path=              render_document  (blocks + toc, no HTML)
///   POST /api/render   body = markdown  render_markdown
///   GET  /api/categories                get_categories
///   GET  /api/search?q=                 search_documents
///   GET  /api/script?name=              find_doc_for_script
///   POST /api/rescan                    rebuild the script map
///   GET  /api/health
#include "document_browser.h"

#include "ergo/studio/host/plugin.h"

#include <memory>

namespace ergo::studio::plugins::musa_thaleia {

class ThaleiaPlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginHealth {
public:
    /// `root` empty = not configured: every API answers 503.
    explicit ThaleiaPlugin(std::filesystem::path root);

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    JsonValue         snapshot() const override;

    /// nullptr when not configured.
    const DocumentBrowser* browser() const { return browser_.get(); }

private:
    host::PluginInfo                 info_;
    std::unique_ptr<DocumentBrowser> browser_;
};

/// Default root: <home>/.ars/spec (as the Tauri app). `home` empty (neither
/// USERPROFILE nor HOME set) leaves the plugin unconfigured instead of
/// guessing a directory.
std::filesystem::path default_thaleia_root(const std::string& home);

std::unique_ptr<host::IPlugin> make_thaleia_plugin(const std::filesystem::path& root, const host::LogFn& log);

} // namespace ergo::studio::plugins::musa_thaleia
