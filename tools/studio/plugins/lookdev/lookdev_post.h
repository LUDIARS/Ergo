#pragma once
/// Posting a look-dev capture: the message text (comment plus what was
/// changed) and the Concordia chat post that carries the still and its preset
/// as attachments. Concordia relays the "報告" channel to Discord and Slack.
#include "../studio/endpoints.h"

#include <string>
#include <vector>

namespace ergo::studio::plugins::lookdev {

/// Concordia's chat text limit.
constexpr std::size_t kMaxPostText = 2000;

/// "ルックデヴ: <app>\n<comment>\n変更: a=1 → 2 ..." cut to kMaxPostText (UTF-8 safe).
std::string compose_post_text(const std::string& app, const std::string& comment,
                              const std::vector<std::string>& changes);

class LookdevPoster {
public:
    LookdevPoster(studio::EndpointResolver& endpoints, const studio::ServiceCaller& caller)
        : endpoints_(endpoints), caller_(caller) {}

    /// POST /v1/chat on Concordia (resolved through Excubitor's topology).
    studio::ServiceResult post(const std::string& text, const std::vector<std::string>& attachment_paths) const;

private:
    studio::EndpointResolver&    endpoints_;
    const studio::ServiceCaller& caller_;
};

} // namespace ergo::studio::plugins::lookdev
