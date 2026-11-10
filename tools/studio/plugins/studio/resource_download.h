#pragma once
/// Executing a Curare acquisition plan into a project folder (port of
/// studio/domain/resource-download.ts, ST-INV-07): decide per item by the
/// local file's hash (never overwrite), write to a temporary file, and rename
/// only when its content hash matches the plan.
#include "../common/json_util.h"
#include "ergo/studio/platform/http_client.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace ergo::studio::plugins::studio {

struct SyncItemResult {
    std::string name;
    std::string version_id;
    std::string result;   ///< downloaded | skip_same | conflict | unavailable | failed | invalid_name
    std::string detail;
};
JsonValue to_json_value(const SyncItemResult& r);

/// Content hashes ("sha256:<hex>") of the regular files directly in `dir`
/// (not *.part), at most `limit`.
std::vector<std::string> local_hashes(const std::filesystem::path& dir, std::size_t limit = 1000);

/// `plan` = Curare's {items:[{versionId, name, contentHash, needsDownload, downloadUrl}], ...}.
std::vector<SyncItemResult> execute_plan(const JsonValue& plan, const std::filesystem::path& dir,
                                         platform::IHttpClient& http, int timeout_ms);

} // namespace ergo::studio::plugins::studio
