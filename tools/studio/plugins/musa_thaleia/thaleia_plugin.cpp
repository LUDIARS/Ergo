#include "thaleia_plugin.h"

namespace ergo::studio::plugins::musa_thaleia {
namespace {

net::HttpResponse fail(int status, const std::string& err) {
    return json_response(status, jobj({{"ok", jbool(false)}, {"err", jstr(err)}}));
}

std::string query(const net::HttpRequest& req, const char* key) {
    const auto it = req.query.find(key);
    return it == req.query.end() ? std::string() : it->second;
}

} // namespace

ThaleiaPlugin::ThaleiaPlugin(std::filesystem::path root)
    : info_{"musa_thaleia", "Thaleia (MUSA)", "Ars-Musa Thaleia: spec document tree, viewer, categories and search.",
            "\xF0\x9F\x93\x96"} {
    if (!root.empty()) browser_ = std::make_unique<DocumentBrowser>(std::move(root));
}

net::HttpResponse ThaleiaPlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    if (sub == "/api/health" && req.method == "GET") return json_response(200, snapshot());
    if (!browser_) return fail(503, "document root is not configured (no home directory)");
    const bool get  = req.method == "GET";
    const bool post = req.method == "POST";

    if (sub == "/api/tree" && get) return json_response(200, to_json_value(browser_->file_tree()));
    if (sub == "/api/document" && get) {
        const auto d = browser_->read(query(req, "path"));
        if (!d) return fail(404, "Document not found: " + query(req, "path"));
        return json_response(200, to_json_value(*d));
    }
    if (sub == "/api/render" && get) {
        const auto d = browser_->read(query(req, "path"));
        if (!d) return fail(404, "Document not found: " + query(req, "path"));
        return json_response(200, to_json_value(render(d->content, d->path, d->category)));
    }
    if (sub == "/api/render" && post) return json_response(200, to_json_value(render(req.body, "inline", std::nullopt)));
    if (sub == "/api/categories" && get) {
        JsonValue arr = JsonValue::make_array();
        for (const auto& c : browser_->categories()) arr.push(jstr(c));
        return json_response(200, arr);
    }
    if (sub == "/api/search" && get) {
        JsonValue arr = JsonValue::make_array();
        for (const auto& d : browser_->search(query(req, "q"))) arr.push(to_json_value(d));
        return json_response(200, arr);
    }
    if (sub == "/api/script" && get) {
        const auto p = browser_->doc_for_script(query(req, "name"));
        return json_response(200, p ? jstr(*p) : JsonValue::make_null());
    }
    if (sub == "/api/rescan" && post) {
        browser_->rescan();
        return json_response(200, jobj({{"ok", jbool(true)}}));
    }
    return fail(404, "not found");
}

JsonValue ThaleiaPlugin::snapshot() const {
    if (!browser_) return jobj({{"ok", jbool(false)}, {"err", jstr("document root is not configured")}});
    std::error_code ec;
    const bool exists = std::filesystem::is_directory(browser_->root(), ec);
    JsonValue h = jobj({{"ok", jbool(true)}, {"root", jstr(browser_->root().u8string())}, {"root_exists", jbool(exists)}});
    return h;
}

std::filesystem::path default_thaleia_root(const std::string& home) {
    if (home.empty()) return {};
    return std::filesystem::u8path(home) / ".ars" / "spec";
}

std::unique_ptr<host::IPlugin> make_thaleia_plugin(const std::filesystem::path& root, const host::LogFn&) {
    return std::make_unique<ThaleiaPlugin>(root);
}

} // namespace ergo::studio::plugins::musa_thaleia
