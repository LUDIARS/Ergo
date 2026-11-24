#include "asset_import_plugin.h"

#include "capture_request.h"
#include "../common/mutation_guard.h"
#include "../common/safe_path.h"
#include "ergo/studio/platform/http_client.h"
#include "pictor/visus/visus_serializer.h"

#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>

namespace ergo::studio::plugins::asset_import {
namespace fs = std::filesystem;
namespace {

net::HttpResponse failure(int status, const std::string& error) {
    return json_response(status, jobj({{"error", jstr(error)}}));
}

std::string query(const net::HttpRequest& request, const std::string& key) {
    const auto found = request.query.find(key);
    return found == request.query.end() ? std::string{} : found->second;
}

fs::path model_source(const WorkspaceProfile& profile, const CatalogEntry& entry, const pictor::VisusDesc& desc) {
    if (desc.kind != pictor::VisusKind::MODEL || desc.asset.empty() || profile.import_dir.empty()) return {};
    std::error_code ec;
    const auto root = fs::weakly_canonical(profile.import_dir, ec);
    if (ec) return {};
    const auto candidate = fs::weakly_canonical(entry.directory / fs::u8path(desc.asset), ec);
    if (ec) return {};
    const auto safe = resolve_within(root, candidate.generic_u8string());
    return safe ? *safe : fs::path{};
}

struct Asset {
    CatalogEntry entry;
    std::string definition;
    pictor::VisusDesc visual;
    fs::path source;
    std::string revision;
};

struct ListedAsset {
    CatalogEntry entry;
    pictor::VisusKind kind;
    JsonValue children;
    std::string revision;
};

std::optional<Asset> read_asset(CatalogEntry entry, const WorkspaceProfile& profile,
                                bool check_source = true) {
    auto bytes = read_bounded(entry.visus_path(), kMaxVisusBytes);
    pictor::VisusDesc desc;
    if (!bytes || !pictor::from_visus_json(*bytes, desc) || desc.name != entry.name) return std::nullopt;
    auto source = check_source ? model_source(profile, entry, desc) : fs::path{};
    std::string revision;
    if (check_source && (desc.kind != pictor::VisusKind::MODEL || !source.empty()))
        revision = asset_revision(*bytes, source);
    return Asset{std::move(entry), std::move(*bytes), std::move(desc), std::move(source), std::move(revision)};
}

class AssetImportPlugin final : public host::IPlugin, public host::IPluginHttp {
public:
    AssetImportPlugin(std::shared_ptr<const IProfileSource> profiles, RequestPreview preview, host::LogFn log)
        : profiles_(std::move(profiles)), preview_(std::move(preview)), log_(std::move(log)),
          worker_([this] { scan_loop(); }) {}

    ~AssetImportPlugin() override {
        {
            std::lock_guard<std::mutex> guard(mutex_);
            stop_ = true;
        }
        wake_.notify_one();
        worker_.join();
    }

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp* http() override { return this; }
    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub) override;

private:
    JsonValue scan(const WorkspaceProfile& profile);
    void scan_loop();
    net::HttpResponse list(const fs::path& root) const;

    const host::PluginInfo info_{"asset_import", "Visus 一覧", "取り込んだモデルを Visus 単位で確認する", ""};
    std::shared_ptr<const IProfileSource> profiles_;
    RequestPreview preview_;
    host::LogFn log_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stop_ = false;
    std::vector<ImportIssue> last_issues_;
    std::map<std::string, ListedAsset> last_assets_;
    fs::path last_root_;
    std::thread worker_;
};

JsonValue AssetImportPlugin::scan(const WorkspaceProfile& profile) {
    const auto root = profile.root / kCatalogDirectory;
    FolderAssetCatalog catalog(root);
    std::vector<ImportIssue> issues;
    std::error_code ec;
    if (!profile.import_dir.empty() && fs::is_directory(profile.import_dir, ec)) {
        issues = import_assets(profile.import_dir, catalog);
    } else if (!profile.import_dir.empty()) {
        issues.push_back({profile.import_dir.u8string(), "import folder is unavailable"});
    }
    std::map<std::string, ListedAsset> assets;
    bool queue_available = static_cast<bool>(preview_);
    for (const auto& entry : catalog.entries()) {
        auto asset = read_asset(entry, profile);
        if (!asset) continue;
        if (queue_available && !asset->source.empty() && !asset->revision.empty() &&
            !has_capture(entry, asset->revision)) {
            queue_available = preview_({entry.name, entry.visus_path(), asset->source, asset->revision, true});
        }
        auto children = JsonValue::make_array();
        for (const auto& child : asset->visual.children) children.push(jstr(child.visus));
        assets.emplace(entry.name, ListedAsset{entry, asset->visual.kind, std::move(children),
                                               std::move(asset->revision)});
    }
    last_root_ = root;
    last_issues_ = std::move(issues);
    last_assets_ = std::move(assets);
    return jobj({{"ok", jbool(true)}});
}

void AssetImportPlugin::scan_loop() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stop_) {
        try {
            const auto all = profiles_->profiles();
            if (!all.profiles.empty()) scan(all.profiles.front());
            else {
                last_root_.clear();
                last_issues_.clear();
                last_assets_.clear();
            }
        } catch (const std::exception& error) {
            if (log_) log_("warn", std::string("[asset_import] ") + error.what());
        }
        wake_.wait_for(lock, std::chrono::seconds(5), [this] { return stop_; });
    }
}

net::HttpResponse AssetImportPlugin::list(const fs::path& root) const {
    auto items = JsonValue::make_array();
    auto issues = JsonValue::make_array();
    if (root == last_root_) {
        for (const auto& issue : last_issues_) {
            issues.push(jobj({{"source", jstr(issue.source)}, {"reason", jstr(issue.reason)}}));
        }
        for (const auto& [name, asset] : last_assets_) {
            const bool captured = has_capture(asset.entry, asset.revision);
            auto summary = jobj({{"name", jstr(name)}, {"kind", jstr(pictor::visus_kind_to_str(asset.kind))},
                                 {"children", asset.children}, {"revision", jstr(asset.revision)},
                                 {"captured", jbool(captured)}});
            if (captured) {
                summary.set("thumbnail", jstr("/asset_import/api/thumbnail?name=" + platform::url_encode(name) +
                                              "&revision=" + asset.revision));
            }
            items.push(std::move(summary));
        }
    }
    return json_response(200, jobj({{"items", std::move(items)}, {"issues", std::move(issues)},
                                    {"window", jbool(static_cast<bool>(preview_))}}));
}

net::HttpResponse AssetImportPlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    if (req.method != "GET" && req.method != "POST") return failure(405, "method not allowed");
    if (req.method == "POST") {
        const auto why = check_mutation(req.method, req.header("host"), req.header("origin"), req.header("content-type"));
        if (why) return failure(403, *why);
    }
    std::lock_guard<std::mutex> guard(mutex_);
    try {
        const auto all = profiles_->profiles();
        if (all.profiles.empty()) return failure(404, "no project open");
        const auto& profile = all.profiles.front();
        const auto root = profile.root / kCatalogDirectory;
        FolderAssetCatalog catalog(root);
        if (req.method == "GET" && sub == "/api/list") return list(root);
        if (req.method == "POST" && sub == "/api/import") {
            if (profile.import_dir.empty()) return failure(409, "no import folder configured");
            return json_response(200, scan(profile));
        }
        if (sub != "/api/detail" && sub != "/api/thumbnail" && sub != "/api/preview" && sub != "/api/capture") {
            return failure(404, "not found");
        }
        const auto name = query(req, "name");
        if (req.method == "GET" && sub == "/api/detail") {
            const auto asset = read_asset(catalog.find(name), profile, false);
            if (!asset) return failure(404, "Visus not found");
            JsonValue result;
            if (!ergo::common::jsonm::parse(asset->definition, result)) return failure(422, "invalid Visus");
            return json_response(200, result);
        }
        if (req.method == "GET" && sub == "/api/thumbnail") {
            if (root != last_root_) return failure(404, "uncaptured");
            const auto asset = last_assets_.find(name);
            const auto wanted = query(req, "revision");
            if (asset == last_assets_.end() || (!wanted.empty() && asset->second.revision != wanted))
                return failure(404, "uncaptured");
            const auto png = read_capture(asset->second.entry, asset->second.revision);
            if (!png) return failure(404, "uncaptured");
            net::HttpResponse response;
            response.status = 200;
            response.headers["content-type"] = "image/png";
            response.headers["x-content-type-options"] = "nosniff";
            response.headers["cache-control"] = "no-store";
            response.body = *png;
            return response;
        }
        if (req.method == "POST" && (sub == "/api/preview" || sub == "/api/capture")) {
            const auto asset = read_asset(catalog.find(name), profile);
            if (!asset) return failure(404, "Visus not found");
            if (!preview_) return json_response(200, jobj({{"status", jstr("uncaptured")}}));
            if (asset->source.empty() || asset->revision.empty()) return failure(422, "model source is unavailable");
            if (!preview_({asset->entry.name, asset->entry.visus_path(), asset->source,
                           asset->revision, sub == "/api/capture", true})) return failure(409, "preview queue is full");
            return json_response(202, jobj({{"status", jstr("queued")}}));
        }
        return failure(405, "method not allowed");
    } catch (const std::invalid_argument& error) {
        return failure(400, error.what());
    } catch (const std::exception& error) {
        return failure(500, error.what());
    }
}

} // namespace

std::unique_ptr<host::IPlugin> make_asset_import_plugin(std::shared_ptr<const IProfileSource> profiles,
                                                       RequestPreview preview, const host::LogFn& log) {
    return std::make_unique<AssetImportPlugin>(std::move(profiles), std::move(preview), log);
}

} // namespace ergo::studio::plugins::asset_import
