#pragma once

/// CommandQueue — multi-producer / single-consumer write-command buffer.
///
/// Inspector clients (network threads) push WriteCommand entries; the host
/// drains them once per frame on the main thread via `Inspector::apply_pending_writes`.
/// Backed by a mutex + std::vector — simple and safe; lock-free can come later
/// if profiling shows it's needed.

#include "ergo/inspector/types.h"

#include <cstdint>
#include <mutex>
#include <vector>

namespace ergo::inspector {

using Handle = uint32_t;
constexpr Handle INVALID_HANDLE = 0;

struct WriteCommand {
    Handle handle = INVALID_HANDLE;
    Value  value;
};

class CommandQueue {
public:
    void push(WriteCommand cmd);

    /// Atomically swap the internal buffer with an empty one and return the contents.
    std::vector<WriteCommand> drain();

    /// Number of pending commands. Useful in tests; race-prone in general.
    std::size_t size_approx() const;

    /// Discard everything without returning it.
    void clear();

private:
    mutable std::mutex        mtx_;
    std::vector<WriteCommand> buf_;
};

} // namespace ergo::inspector
