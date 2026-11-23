#pragma once
/// "git" plugin: version control of the game workspaces from the Studio (ST-INV-10).
/// The repositories are the ones the open project's profile lists; a request names
/// one by its index in GET /api/repos, never by a path, so no other folder can
/// be reached. Not part of the Revisor workflow (branches pushed here are
/// ordinary git pushes; the Revisor guard still applies where it is installed).
///   GET  /api/repos                         [{index, path, profile}]
///   GET  /api/repos/<i>/status              {branch, upstream, ahead, behind, files}
///   GET  /api/repos/<i>/log                 the last 30 commits
///   GET  /api/repos/<i>/branches
///   POST /api/repos/<i>/fetch | pull | push
///   POST /api/repos/<i>/switch              {branch, create?}
///   POST /api/repos/<i>/commit              {message}: stages every change and commits
#include "git_ops.h"
#include "../common/profile_source.h"

#include <memory>

#include "ergo/studio/host/plugin.h"

namespace ergo::studio::plugins::git {

class GitPlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginHealth {
public:
    GitPlugin(GitOps ops, std::shared_ptr<const IProfileSource> profiles, host::LogFn log);

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    JsonValue         snapshot() const override;

private:
    struct Repo {
        std::filesystem::path path;
        std::string           profile;
    };
    std::vector<Repo> repos() const;
    net::HttpResponse act(const Repo& repo, const std::string& what, const net::HttpRequest& req);

    host::PluginInfo                      info_;
    GitOps                                ops_;
    std::shared_ptr<const IProfileSource> profiles_;
    host::LogFn                           log_;
};

std::unique_ptr<host::IPlugin> make_git_plugin(std::shared_ptr<const IProfileSource> profiles, int timeout_ms,
                                               const host::LogFn& log);

} // namespace ergo::studio::plugins::git
