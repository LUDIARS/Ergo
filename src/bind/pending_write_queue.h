#pragma once

/// PendingWriteQueue — thread-safe handoff of inbound `set` requests.
///
/// Single responsibility: buffer writes that arrive on the WS worker thread
/// until the host drains them on its own thread in apply_pending_writes().

#include "ergo/bind/types.h"

#include <mutex>
#include <utility>
#include <vector>

namespace ergo::bind::detail {

class PendingWriteQueue {
public:
    struct PendingWrite {
        Handle handle = INVALID_HANDLE;
        Value  value;
    };

    void push(Handle h, Value v) {
        std::lock_guard<std::mutex> lk(mtx_);
        wq_.push_back({h, std::move(v)});
    }

    std::vector<PendingWrite> drain() {
        std::vector<PendingWrite> out;
        std::lock_guard<std::mutex> lk(mtx_);
        out.swap(wq_);
        return out;
    }

private:
    std::mutex                mtx_;
    std::vector<PendingWrite> wq_;
};

} // namespace ergo::bind::detail
