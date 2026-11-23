#pragma once
/// Builds and runs play targets: one child at a time per target (a build or
/// the game), its output kept in a TargetLog, a build stopped at its
/// timeout. The command is always the profile's (PLAY-INV-01).
#include "play_target.h"
#include "target_log.h"
#include "ergo/studio/platform/live_process.h"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace ergo::studio::plugins::play {

enum class TargetState { Idle, Building, Running, Failed };
const char* state_name(TargetState s);

struct TargetStatus {
    TargetState        state = TargetState::Idle;
    std::optional<int> exit_code;   ///< of the last build / run that ended
};

enum class StartResult { Started, Busy, NoBuild, Failed };

class TargetRunner {
public:
    using StartFn = std::function<std::unique_ptr<platform::ILiveProcess>(
        const platform::LiveProcessSpec&, platform::LiveProcessHandlers, std::string&)>;
    /// Called after a target's state changed (not under the runner's lock).
    using StateFn = std::function<void(const std::string& target, const TargetStatus& status)>;

    TargetRunner(StartFn start, StateFn on_state);
    ~TargetRunner();
    TargetRunner(const TargetRunner&)            = delete;
    TargetRunner& operator=(const TargetRunner&) = delete;

    StartResult build(const PlayTarget& target, std::string& err);
    StartResult run(const PlayTarget& target, std::string& err);
    /// False when nothing of that target was running.
    bool         stop(const std::string& name);
    TargetStatus status(const std::string& name) const;
    std::vector<LogLine> logs(const std::string& name, uint64_t since, uint64_t& next) const;

private:
    struct Slot {
        TargetStatus                           status;
        std::unique_ptr<platform::ILiveProcess> process;
        TargetLog                              log;
        uint64_t                               generation = 0;
        bool                                   building   = false;
        std::optional<std::chrono::steady_clock::time_point> deadline;
    };

    StartResult start(const PlayTarget& target, const PlayCommand& command, bool building, std::string& err);
    void        finished(const std::string& name, uint64_t generation, int exit_code);
    void        watchdog();

    StartFn                     start_;
    StateFn                     on_state_;
    mutable std::mutex          mutex_;
    std::map<std::string, Slot> slots_;
    std::condition_variable     wake_;
    bool                        quitting_ = false;
    std::thread                 watchdog_;
};

} // namespace ergo::studio::plugins::play
