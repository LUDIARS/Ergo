#include "pictor_perf_plugin.h"

#include "../common/json_util.h"
#include "../common/latest_blob_plugin.h"

namespace ergo::studio::plugins::pictor_perf {

bool is_perf_snapshot(const std::string& doc) {
    JsonValue v;
    return ergo::common::jsonm::parse(doc, v) && v.is_object() && v.find("memory") != nullptr;
}

std::unique_ptr<host::IPlugin> make_pictor_perf_plugin(const host::LogFn& log) {
    LatestBlobSpec s;
    s.info          = {"pictor_perf", "Pictor Perf Monitor",
                       "Pictor の per-object/batch 性能・DoD キャッシュ効率・GPU バッチ適格性を可視化。",
                       "\xF0\x9F\x93\x8A"};
    s.frame_op      = "snapshot";
    s.field         = "snapshot";
    s.has_key       = "has_snapshot";
    s.http_publish  = true;
    s.validate      = &is_perf_snapshot;
    s.invalid_error = "invalid perf snapshot (memory フィールド必須)";
    return std::make_unique<LatestBlobPlugin>(std::move(s), log);
}

} // namespace ergo::studio::plugins::pictor_perf
