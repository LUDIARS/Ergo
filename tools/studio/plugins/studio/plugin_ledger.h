#pragma once
/// Ledger of fetched plugin packs, <studio home>/plugins/ledger.json (port of
/// tools/ergo core/plugin-ledger.ts; same JSON). It records what was approved
/// and installed; it does not fetch or check anything. At start the enabled
/// packs are appended to ERGO_PLUGIN_DIR and load like any other pack.
#include "../common/json_util.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ergo::studio::plugins::studio {

constexpr int kLedgerVersion = 1;

struct LedgerPack {
    std::string              id;            ///< "<owner>/<repo>:<subpath>"
    std::string              owner, repo, subpath;
    std::string              rev;           ///< 40-hex sha
    std::string              checkout_dir;
    bool                     enabled = false;
    std::string              installed_at;
    std::vector<std::string> plugins;       ///< names shown at approval
};
JsonValue to_json_value(const LedgerPack& p);

struct Ledger {
    std::vector<LedgerPack> packs;
};

std::filesystem::path ledger_path(const std::filesystem::path& home);
/// Broken entries are dropped (start-up never stops on a bad ledger).
Ledger parse_ledger(const std::string& text);
Ledger read_ledger(const std::filesystem::path& home);
/// Written to a temporary file first, then renamed (no half-written ledger).
bool write_ledger(const std::filesystem::path& home, const Ledger& l, std::string& err);

/// <checkout_dir>/<subpath> of the enabled packs.
std::vector<std::string> enabled_pack_dirs(const Ledger& l);
/// `raw` (ERGO_PLUGIN_DIR) with the enabled packs appended (no duplicates).
std::string plugin_dirs_with_ledger(const std::string& raw, const Ledger& l);

} // namespace ergo::studio::plugins::studio
