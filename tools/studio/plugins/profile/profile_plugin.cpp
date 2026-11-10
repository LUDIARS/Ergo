#include "profile_plugin.h"

#include "../common/latest_blob_plugin.h"

namespace ergo::studio::plugins::profile {

std::unique_ptr<host::IPlugin> make_profile_plugin(const host::LogFn& log) {
    LatestBlobSpec s;
    s.info     = {"profile", "Profile Timeline",
                  "Drop an ergo_profile Chrome trace (.json) to inspect the performance timeline.", "\xE2\x8F\xB1"};
    s.frame_op = "trace";
    s.field    = "trace";
    s.has_key  = "has_trace";
    return std::make_unique<LatestBlobPlugin>(std::move(s), log);
}

} // namespace ergo::studio::plugins::profile
