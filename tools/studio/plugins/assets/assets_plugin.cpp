#include "assets_plugin.h"

#include "asset_catalog.h"
#include "../common/safe_path.h"

#include <charconv>
#include <fstream>
#include <sstream>

namespace ergo::studio::plugins::assets {
namespace fs = std::filesystem;

namespace {

net::HttpResponse fail(int status, const std::string& err) {
    return json_response(status, jobj({{"ok", jbool(false)}, {"err", jstr(err)}}));
}

std::string query(const net::HttpRequest& req, const std::string& key) {
    const auto it = req.query.find(key);
    return it == req.query.end() ? std::string() : it->second;
}

} // namespace

AssetsPlugin::AssetsPlugin(std::string home, std::string plugin_dirs, host::LogFn log)
    : info_{"assets", "Assets", "ゲームのアセット (ワークスペースのプロフィールの assets) を一覧し、画像と文字のファイルを見る。読み取りのみ。",
            "\xF0\x9F\x97\x82"},
      home_(std::move(home)),
      plugin_dirs_(std::move(plugin_dirs)),
      log_(std::move(log)) {}

std::vector<AssetsPlugin::Root> AssetsPlugin::roots() const {
    std::vector<Root> out;
    for (const auto& p : load_workspace_profiles(fs::u8path(home_), plugin_dirs_, &process_env).profiles) {
        for (const auto& r : p.assets) out.push_back({p.name, r});
    }
    return out;
}

JsonValue AssetsPlugin::snapshot() const {
    const auto list = roots();
    JsonValue out = jobj({{"ok", jbool(!list.empty())}, {"roots", jnum(static_cast<double>(list.size()))}});
    if (list.empty()) out.set("err", jstr("no workspace profile lists \"assets\" (profiles: <studio home>/profiles, ERGO_PLUGIN_DIR)"));
    return out;
}

net::HttpResponse AssetsPlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    if (req.method != "GET") return fail(405, "method not allowed (the asset list is read only)");
    if (sub == "/api/health") return json_response(200, snapshot());
    if (sub == "/api/roots") {
        const auto list = roots();
        JsonValue out = JsonValue::make_array();
        for (std::size_t i = 0; i < list.size(); ++i) {
            std::error_code ec;
            out.push(jobj({{"index", jnum(static_cast<double>(i))},
                           {"profile", jstr(list[i].profile)},
                           {"name", jstr(list[i].root.name)},
                           {"path", jstr(list[i].root.path.u8string())},
                           {"exists", jbool(fs::is_directory(list[i].root.path, ec))}}));
        }
        return json_response(200, out);
    }
    if (sub == "/api/list") return list(req);
    if (sub == "/api/file") return file(req);
    return fail(404, "not found");
}

net::HttpResponse AssetsPlugin::list(const net::HttpRequest& req) const {
    const auto all = roots();
    std::size_t index = 0;
    const std::string n = query(req, "root");
    const auto r = std::from_chars(n.data(), n.data() + n.size(), index);
    if (n.empty() || r.ec != std::errc{} || r.ptr != n.data() + n.size() || index >= all.size()) return fail(404, "no such asset folder");
    const std::string dir = query(req, "dir");
    std::string err;
    const auto listing = list_folder(all[index].root.path, dir, err);
    if (!listing) return fail(404, err);
    return json_response(200, listing_json(dir, *listing));
}

net::HttpResponse AssetsPlugin::file(const net::HttpRequest& req) const {
    const auto all = roots();
    std::size_t index = 0;
    const std::string n = query(req, "root");
    const auto r = std::from_chars(n.data(), n.data() + n.size(), index);
    if (n.empty() || r.ec != std::errc{} || r.ptr != n.data() + n.size() || index >= all.size()) return fail(404, "no such asset folder");
    const auto path = resolve_within(all[index].root.path, query(req, "path"));
    if (!path) return fail(404, "path is outside the asset folder");
    std::error_code ec;
    if (!fs::is_regular_file(*path, ec)) return fail(404, "not a file");
    const auto type = preview_type(*path);
    if (!type) return fail(415, "no preview for this kind of file");
    const auto size = fs::file_size(*path, ec);
    if (ec || size > kMaxPreviewBytes) return fail(413, "file too large to preview");
    std::ifstream in(*path, std::ios::binary);
    if (!in) return fail(500, "cannot read the file");
    std::ostringstream s;
    s << in.rdbuf();
    net::HttpResponse res;
    res.status = 200;
    res.body = s.str();
    res.headers["content-type"] = *type;
    res.headers["x-content-type-options"] = "nosniff";
    res.headers["content-security-policy"] = "default-src 'none'; sandbox";
    res.headers["cache-control"] = "no-store";
    return res;
}

std::unique_ptr<host::IPlugin> make_assets_plugin(const std::string& studio_home, const std::string& plugin_dirs,
                                                  const host::LogFn& log) {
    return std::make_unique<AssetsPlugin>(studio_home, plugin_dirs, log);
}

} // namespace ergo::studio::plugins::assets
