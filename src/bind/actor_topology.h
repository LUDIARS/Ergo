#pragma once

/// ActorTopology — cache of scene-graph actor nodes.
///
/// Single responsibility: remember the actors announced via actor_register /
/// actor_unregister so the engine can replay them to the tool on reconnect.
/// Thread-safe via its own mutex; no wire-protocol knowledge.

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ergo::bind::detail {

class ActorTopology {
public:
    struct ActorEntry {
        uint64_t    handle = 0;
        uint64_t    parent = 0;
        std::string name;
    };

    /// Insert or update an actor; returns a copy of the stored entry so the
    /// caller can send it immediately.
    ActorEntry upsert(uint64_t handle, uint64_t parent, std::string name) {
        ActorEntry a{handle, parent, std::move(name)};
        std::lock_guard<std::mutex> lk(mtx_);
        actors_[handle] = a;
        return a;
    }

    /// Remove an actor. Returns true if it was present.
    bool erase(uint64_t handle) {
        std::lock_guard<std::mutex> lk(mtx_);
        return actors_.erase(handle) > 0;
    }

    std::vector<ActorEntry> snapshot() const {
        std::lock_guard<std::mutex> lk(mtx_);
        std::vector<ActorEntry> out;
        out.reserve(actors_.size());
        for (auto& [h, a] : actors_) out.push_back(a);
        return out;
    }

private:
    mutable std::mutex                       mtx_;
    std::unordered_map<uint64_t, ActorEntry> actors_;
};

} // namespace ergo::bind::detail
