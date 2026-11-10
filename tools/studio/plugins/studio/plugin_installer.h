#pragma once
/// Plugin pack fetch: preview -> approval -> install (port of
/// studio/domain/plugin-installer.ts, ST-INV-08 / design J6).
///  1. preview: resolve rev to a 40-hex sha, sparse-fetch only `subpath` at
///     that sha into a staging dir, list its plugins (subdirectories with a
///     CMakeLists.txt) and show commit / names / replaced rev.
///  2. install: move exactly the previewed staging dir (no re-fetch), build
///     each plugin with CMake against the Studio SDK (plugin_abi.h) into
///     <plugin>/ERGO_STUDIO_PLUGIN_FILE, and pin the sha in the ledger. The
///     JS version ran `npm install --ignore-scripts`; native packs are built.
///  3. loaded at the next start, never into the running process.
#include "plugin_ledger.h"
#include "plugin_source.h"

#include "ergo/studio/platform/process_runner.h"

#include <chrono>
#include <map>
#include <memory>
#include <mutex>

namespace ergo::studio::plugins::studio {

struct PluginPreview {
    std::string              token, id, owner, repo, subpath;
    std::string              rev;         ///< resolved sha
    std::string              requested;   ///< rev as asked
    std::string              commit_subject, commit_date;
    std::vector<std::string> plugins;
    std::optional<std::string> replaces;  ///< rev of the pack it replaces
    std::string              expires_at;
};
JsonValue to_json_value(const PluginPreview& p);

class PluginInstaller {
public:
    /// `sdk_dir` holds include/ergo/studio/plugin_abi.h (shipped next to ergo_studio.exe).
    PluginInstaller(std::filesystem::path home, std::filesystem::path sdk_dir,
                    std::shared_ptr<platform::IProcessRunner> runner, int timeout_ms);

    /// nullopt with `err` on failure (the staging dir is removed).
    std::optional<PluginPreview> preview(const PluginSource& s, std::string& err);
    std::optional<LedgerPack>    install(const std::string& token, std::string& err);
    std::optional<LedgerPack>    set_enabled(const std::string& id, bool enabled, std::string& err);
    bool                         remove(const std::string& id, std::string& err);
    std::vector<LedgerPack>      list() const;

private:
    struct Staged {
        PluginPreview                          preview;
        std::filesystem::path                  dir;
        std::chrono::steady_clock::time_point  at;
    };
    platform::ProcessResult run(const std::string& program, const std::vector<std::string>& args,
                                const std::filesystem::path& cwd, int timeout_ms) const;
    bool        git(const std::vector<std::string>& args, const std::filesystem::path& cwd, std::string& out, std::string& err) const;
    std::optional<std::string> resolve_rev(const PluginSource& s, std::string& err) const;
    bool        build_plugin(const std::filesystem::path& dir, std::string& err) const;
    void        sweep();

    std::filesystem::path                     home_;
    std::filesystem::path                     sdk_dir_;
    std::shared_ptr<platform::IProcessRunner> runner_;
    int                                       timeout_ms_;
    std::mutex                                mtx_;
    std::map<std::string, Staged>             staged_;
};

} // namespace ergo::studio::plugins::studio
