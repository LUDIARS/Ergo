#include "plugin_registry.h"

#include "../common/file_io.h"

#include <algorithm>
#include <fstream>

#ifdef ERGO_STUDIO_HAS_REGISTRY_ROOT
extern const char ergo_plugin_registry_root[];
extern const std::size_t ergo_plugin_registry_root_size;
#endif

namespace ergo::studio::plugins::studio {
namespace {

std::optional<std::uint64_t> read_serial(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return ec ? std::nullopt : std::optional<std::uint64_t>(0);
    std::ifstream input(path, std::ios::binary);
    char bytes[17];
    input.read(bytes, sizeof(bytes));
    std::string text(bytes, static_cast<std::size_t>(input.gcount()));
    if (!input.eof() || input.bad() || text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
        return std::nullopt;
    return std::stoull(text);
}

bool persist_serial(const std::filesystem::path& path, std::uint64_t serial) {
    auto temp = path;
    temp += ".tmp";
    std::string error;
    if (!write_text_file(temp, std::to_string(serial), error)) return false;
    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    return !ec;
}

} // namespace

PluginRegistry::PluginRegistry(std::filesystem::path home, std::string root,
                               std::unique_ptr<registry::IAssetTransport> transport, int timeout_ms)
    : home_(std::move(home)), root_(std::move(root)), transport_(std::move(transport)), timeout_ms_(timeout_ms) {}

registry::Trust PluginRegistry::refresh(bool require_fresh) {
    std::lock_guard<std::mutex> lock(mutex_);
    using Clock = std::chrono::steady_clock;
    if (!require_fresh && cached_at_ != Clock::time_point{} && Clock::now() - cached_at_ < std::chrono::seconds(15)) return cached_;
    cached_ = fetch_and_verify();
    cached_at_ = Clock::now();
    return cached_;
}

registry::Trust PluginRegistry::fetch_and_verify() {
    registry::Trust trust;
    if (home_.empty()) {
        trust.reason = "Studio の保存先が設定されていません";
        return trust;
    }
    const auto release = registry::fetch_release(*transport_, timeout_ms_, !root_.empty());
    if (!release) {
        trust.reason = root_.empty() ? "信頼の起点がありません。索引も取得できませんでした" : "索引を取得できません";
        return trust;
    }
    const auto path = ledger_path(home_).parent_path() / "registry-serial";
    const auto stored = read_serial(path);
    std::uint64_t minimum = stored.value_or(0);
    for (const auto& pack : read_ledger(home_).packs) {
        if (pack.trust) minimum = std::max(minimum, pack.trust->serial);
    }
    trust = registry::verify_release(*release, root_, minimum, std::time(nullptr));
    if (trust.verified() && (!stored || (trust.serial > *stored && !persist_serial(path, trust.serial))))
        trust.reason = "索引の版を保存するか確認できません";
    return trust;
}

void PluginRegistry::filter_startup(Ledger& ledger) {
    const bool has_signed = std::any_of(ledger.packs.begin(), ledger.packs.end(), [](const LedgerPack& pack) {
        return pack.enabled && pack.trust.has_value();
    });
    if (!has_signed) return;
    if (root_.empty()) {
        for (auto& pack : ledger.packs) {
            if (pack.trust) pack.enabled = false;
        }
        return;
    }
    const auto trust = refresh();
    for (auto& pack : ledger.packs) {
        if (pack.enabled && pack.trust && !trust.permits(*pack.trust, pack.rev)) pack.enabled = false;
    }
}

std::unique_ptr<PluginRegistry> make_registry(const std::filesystem::path& home, int timeout_ms) {
    std::string root;
#ifdef ERGO_STUDIO_HAS_REGISTRY_ROOT
    root.assign(ergo_plugin_registry_root, ergo_plugin_registry_root_size);
#endif
    return std::make_unique<PluginRegistry>(home, std::move(root), registry::make_portus_transport(), timeout_ms);
}

std::string registry_plugin_dirs(const std::string& raw, const std::filesystem::path& home, int timeout_ms) {
    if (home.empty()) return raw;
    auto ledger = read_ledger(home);
    make_registry(home, timeout_ms)->filter_startup(ledger);
    return plugin_dirs_with_ledger(raw, ledger);
}

} // namespace ergo::studio::plugins::studio
