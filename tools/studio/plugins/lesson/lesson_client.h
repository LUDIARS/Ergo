#pragma once
/// The three odeum-lesson calls (pair / state / progress) over IHttpClient.
/// Blocking; call it off the UI thread. The token and the link code go into
/// the request only: they are never part of a returned failure.
#include "lesson_link.h"
#include "lesson_protocol.h"

#include <memory>

namespace ergo::studio::plugins::lesson {

template <class T>
struct CallResult {
    std::optional<T> value;
    CallFailure      failure;
    bool             ok() const { return value.has_value(); }
};

class LessonClient {
public:
    explicit LessonClient(std::shared_ptr<platform::IHttpClient> http, int timeout_ms = 5000);

    /// POST <origin>/v1/tool/pair
    CallResult<PairReply>   pair(const LessonLink& link) const;
    /// GET <origin>/v1/tool/state
    CallResult<LessonState> state(const std::string& origin, const std::string& token) const;
    /// POST <origin>/v1/tool/progress (done=true). FailureKind::None on success.
    CallFailure progress(const std::string& origin, const std::string& token, const std::string& assignment_id,
                         std::size_t step) const;

private:
    std::shared_ptr<platform::IHttpClient> http_;
    int                                    timeout_ms_;
};

} // namespace ergo::studio::plugins::lesson
