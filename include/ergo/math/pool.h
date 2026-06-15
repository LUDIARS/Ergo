#pragma once

/// ergo::math::pool — DoD-oriented allocators.
///
///   Arena        — bump allocator over a contiguous byte buffer.
///                  Allocates aligned storage and constructs T with placement
///                  new. No per-object deallocation; reset() reclaims all.
///
///   ObjectPool<T> — fixed-capacity pool with stable handles.
///                  Uses a single contiguous T[] slab; placement new constructs,
///                  explicit destroy() defers to a free-list for reuse.
///                  Per-element heap allocation: zero.
///
/// Both classes are move-only (no copy). Thread safety: none.
///
/// Spec: spec/module/math.md

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>    // std::memset
#include <memory>     // std::align
#include <new>        // placement new, std::launder
#include <type_traits>
#include <utility>    // std::forward, std::move
#include <vector>

namespace ergo::math {

// ---------------------------------------------------------------------------
// Arena — linear bump allocator
// ---------------------------------------------------------------------------

/// Fixed-size arena. Storage is backed by an internal byte array.
/// `capacity` bytes are allocated up front.
class Arena {
public:
    explicit Arena(std::size_t capacity)
        : buf_(new std::byte[capacity]),
          capacity_(capacity),
          offset_(0) {}

    // Move-only
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    Arena(Arena&& o) noexcept
        : buf_(std::move(o.buf_)),
          capacity_(o.capacity_),
          offset_(o.offset_) {
        o.capacity_ = 0;
        o.offset_ = 0;
    }

    ~Arena() = default;  // unique_ptr cleans up

    /// Allocate size bytes with the given alignment. Returns nullptr on overflow.
    void* alloc(std::size_t size, std::size_t alignment = alignof(std::max_align_t)) noexcept {
        void*        ptr  = buf_.get() + offset_;
        std::size_t  space = capacity_ - offset_;
        if (std::align(alignment, size, ptr, space)) {
            // ptr was adjusted; compute new offset
            std::size_t new_offset =
                static_cast<std::byte*>(ptr) - buf_.get() + size;
            offset_ = new_offset;
            return ptr;
        }
        return nullptr;
    }

    /// Construct T in-place. Returns nullptr on overflow.
    template <class T, class... Args>
    T* construct(Args&&... args) {
        void* mem = alloc(sizeof(T), alignof(T));
        if (!mem) return nullptr;
        return ::new (mem) T(std::forward<Args>(args)...);
    }

    /// Reclaim all memory (does NOT call any destructors).
    void reset() noexcept { offset_ = 0; }

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::size_t used()     const noexcept { return offset_; }
    [[nodiscard]] std::size_t remaining() const noexcept { return capacity_ - offset_; }

private:
    std::unique_ptr<std::byte[]> buf_;
    std::size_t capacity_{0};
    std::size_t offset_{0};
};

// ---------------------------------------------------------------------------
// ObjectPool<T> — fixed-capacity pool with free-list
// ---------------------------------------------------------------------------

/// Stable-handle pool for up to `capacity` objects of type T.
/// All storage is a single contiguous aligned slab (no per-element alloc).
///
/// Handles are integer indices (Handle = std::size_t).
/// A handle is valid until destroy() is called on it.
template <class T>
class ObjectPool {
public:
    using Handle = std::size_t;
    static constexpr Handle invalid_handle = ~Handle(0);

    explicit ObjectPool(std::size_t capacity)
        : capacity_(capacity),
          slab_(static_cast<T*>(::operator new[](sizeof(T) * capacity,
                                                  std::align_val_t{alignof(T)}))) {
        // Initialise free-list: 0, 1, 2, ... capacity-1
        free_list_.reserve(capacity);
        for (std::size_t i = capacity; i-- > 0; )
            free_list_.push_back(i);
        alive_.assign(capacity, false);
    }

    ~ObjectPool() {
        // Destroy surviving objects
        for (std::size_t i = 0; i < capacity_; ++i)
            if (alive_[i]) std::launder(slab_ + i)->~T();
        ::operator delete[](slab_, std::align_val_t{alignof(T)});
    }

    // Move-only
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    ObjectPool(ObjectPool&& o) noexcept
        : capacity_(o.capacity_),
          slab_(o.slab_),
          alive_(std::move(o.alive_)),
          free_list_(std::move(o.free_list_)) {
        o.slab_     = nullptr;
        o.capacity_ = 0;
    }

    /// Construct a T. Returns invalid_handle if pool is full.
    template <class... Args>
    [[nodiscard]] Handle create(Args&&... args) {
        if (free_list_.empty()) return invalid_handle;
        Handle h = free_list_.back();
        free_list_.pop_back();
        ::new (slab_ + h) T(std::forward<Args>(args)...);
        alive_[h] = true;
        return h;
    }

    /// Destroy the object at handle h. After this call h is invalid.
    void destroy(Handle h) {
        assert(h < capacity_ && alive_[h] && "double-destroy or invalid handle");
        std::launder(slab_ + h)->~T();
        alive_[h] = false;
        free_list_.push_back(h);
    }

    /// Access the object at handle h. UB if h is not alive.
    [[nodiscard]] T& get(Handle h) noexcept {
        assert(h < capacity_ && alive_[h]);
        return *std::launder(slab_ + h);
    }
    [[nodiscard]] const T& get(Handle h) const noexcept {
        assert(h < capacity_ && alive_[h]);
        return *std::launder(slab_ + h);
    }

    /// Pointer to the raw slab (contiguous storage, may contain holes).
    [[nodiscard]] const T* slab() const noexcept { return slab_; }
    [[nodiscard]] T*       slab()       noexcept { return slab_; }

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::size_t size()     const noexcept {
        return capacity_ - free_list_.size();
    }
    [[nodiscard]] bool alive(Handle h) const noexcept {
        return h < capacity_ && alive_[h];
    }

private:
    std::size_t      capacity_{0};
    T*               slab_{nullptr};
    std::vector<bool> alive_;
    std::vector<Handle> free_list_;
};

}  // namespace ergo::math
