#pragma once

/// PatchQueue — thread-safe handoff of inbound scene_editor `patch` frames.
///
/// Single responsibility: buffer patches that arrive on the WS worker thread
/// until the host applies them on its own thread in EditorLink::poll(). Same
/// shape as ergo_bind's PendingWriteQueue, plus a bound so a host that never
/// polls cannot grow it without limit (the oldest patch is dropped first).

#include <cstddef>
#include <deque>
#include <iterator>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace ergo::scene::detail {

struct Patch {
    std::string scene_id;
    std::string actor_id;
    std::string field;
    std::string value_json;  // serialized JSON of the patch's `value`
};

class PatchQueue {
public:
    static constexpr std::size_t kMaxPending = 4096;

    void push(Patch p) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (q_.size() >= kMaxPending) q_.pop_front();
        q_.push_back(std::move(p));
    }

    std::vector<Patch> drain() {
        std::lock_guard<std::mutex> lk(mtx_);
        std::vector<Patch> out(std::make_move_iterator(q_.begin()), std::make_move_iterator(q_.end()));
        q_.clear();
        return out;
    }

    /// Put patches that were drained but not consumed back in front of any
    /// that arrived meanwhile, keeping arrival order.
    void requeue_front(std::vector<Patch> ps) {
        if (ps.empty()) return;
        std::lock_guard<std::mutex> lk(mtx_);
        q_.insert(q_.begin(), std::make_move_iterator(ps.begin()), std::make_move_iterator(ps.end()));
        while (q_.size() > kMaxPending) q_.pop_front();
    }

    /// Discard every queued patch for `scene_id`.
    void discard(const std::string& scene_id) {
        std::lock_guard<std::mutex> lk(mtx_);
        for (auto it = q_.begin(); it != q_.end();) {
            it = (it->scene_id == scene_id) ? q_.erase(it) : it + 1;
        }
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return q_.size();
    }

private:
    mutable std::mutex mtx_;
    std::deque<Patch>  q_;
};

} // namespace ergo::scene::detail
