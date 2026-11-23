#include "target_runner.h"

namespace ergo::studio::plugins::play {

const char* state_name(TargetState s) {
    switch (s) {
        case TargetState::Idle: return "idle";
        case TargetState::Building: return "building";
        case TargetState::Running: return "running";
        case TargetState::Failed: return "failed";
    }
    return "idle";
}

TargetRunner::TargetRunner(StartFn start, StateFn on_state)
    : start_(std::move(start)), on_state_(std::move(on_state)), watchdog_([this] { watchdog(); }) {}

TargetRunner::~TargetRunner() {
    std::vector<std::unique_ptr<platform::ILiveProcess>> processes;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        quitting_ = true;
        for (auto& [name, slot] : slots_) {
            if (slot.process) processes.push_back(std::move(slot.process));
        }
    }
    wake_.notify_all();
    if (watchdog_.joinable()) watchdog_.join();
    // Destroyed outside the lock: their exit handlers take it.
    processes.clear();
}

StartResult TargetRunner::build(const PlayTarget& target, std::string& err) {
    if (!target.build) {
        err = "target \"" + target.name + "\" has no build command";
        return StartResult::NoBuild;
    }
    return start(target, *target.build, true, err);
}

StartResult TargetRunner::run(const PlayTarget& target, std::string& err) {
    return start(target, target.run, false, err);
}

StartResult TargetRunner::start(const PlayTarget& target, const PlayCommand& command, bool building, std::string& err) {
    std::unique_ptr<platform::ILiveProcess> previous;
    TargetStatus                             status;
    StartResult                              result = StartResult::Started;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (quitting_) {
            err = "the Studio is shutting down";
            return StartResult::Failed;
        }
        Slot& slot = slots_[target.name];
        if (slot.process && slot.process->running()) {
            err = std::string("target is ") + (slot.building ? "building" : "running");
            return StartResult::Busy;
        }
        previous = std::move(slot.process);   // an ended one; destroyed after the lock
        const uint64_t generation = ++slot.generation;
        slot.building = building;
        slot.log.push(false, std::string("> ") + (building ? "build: " : "run: ") + command.cmd);

        platform::LiveProcessHandlers handlers;
        const std::string name = target.name;
        handlers.on_line = [this, name, generation](bool is_stderr, std::string_view line) {
            std::lock_guard<std::mutex> l(mutex_);
            auto it = slots_.find(name);
            if (it != slots_.end() && it->second.generation == generation) it->second.log.push(is_stderr, line);
        };
        handlers.on_exit = [this, name, generation](int code) { finished(name, generation, code); };
        platform::LiveProcessSpec spec{command.cmd, command.args, target.root};
        slot.process = start_(spec, std::move(handlers), err);
        if (!slot.process) {
            slot.status = {TargetState::Failed, std::nullopt};
            slot.log.push(true, "could not start: " + err);
            result = StartResult::Failed;
        } else {
            slot.status = {building ? TargetState::Building : TargetState::Running, std::nullopt};
            slot.deadline.reset();
            if (building && command.timeout_sec > 0)
                slot.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(command.timeout_sec);
        }
        status = slot.status;
    }
    wake_.notify_all();
    previous.reset();
    if (on_state_) on_state_(target.name, status);
    return result;
}

void TargetRunner::finished(const std::string& name, uint64_t generation, int exit_code) {
    TargetStatus status;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = slots_.find(name);
        if (it == slots_.end() || it->second.generation != generation) return;
        Slot& slot = it->second;
        slot.deadline.reset();
        const bool failed = slot.building && exit_code != 0;
        slot.status = {failed ? TargetState::Failed : TargetState::Idle, exit_code};
        slot.log.push(exit_code != 0, std::string("> ") + (slot.building ? "build" : "run") + " ended (exit " +
                                          std::to_string(exit_code) + ")");
        status = slot.status;
        if (quitting_) return;
    }
    if (on_state_) on_state_(name, status);
}

bool TargetRunner::stop(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = slots_.find(name);
    if (it == slots_.end() || !it->second.process || !it->second.process->running()) return false;
    it->second.log.push(false, "> stop");
    it->second.process->terminate();
    return true;
}

TargetStatus TargetRunner::status(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = slots_.find(name);
    return it == slots_.end() ? TargetStatus{} : it->second.status;
}

std::vector<LogLine> TargetRunner::logs(const std::string& name, uint64_t since, uint64_t& next) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = slots_.find(name);
    if (it == slots_.end()) {
        next = 0;
        return {};
    }
    return it->second.log.since(since, next);
}

void TargetRunner::watchdog() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!quitting_) {
        const auto now = std::chrono::steady_clock::now();
        for (auto& [name, slot] : slots_) {
            if (slot.deadline && now >= *slot.deadline && slot.process && slot.process->running()) {
                slot.log.push(true, "> build timed out");
                slot.deadline.reset();
                slot.process->terminate();
            }
        }
        wake_.wait_for(lock, std::chrono::seconds(1));
    }
}

} // namespace ergo::studio::plugins::play
