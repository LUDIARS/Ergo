#include "ergo/actor/actor.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace ergo::actor {

namespace {

struct Registry {
    std::mutex                            mtx;
    std::unordered_map<Handle, Actor*>    by_handle;
};

Registry& registry() {
    static Registry r;
    return r;
}

std::atomic<Handle>& handle_counter() {
    // Start at 1 so 0 (INVALID_HANDLE) is reserved as "global / no owner".
    static std::atomic<Handle> c{1};
    return c;
}

} // namespace

namespace detail {

Handle allocate_handle() {
    return handle_counter().fetch_add(1, std::memory_order_relaxed);
}

void add(Actor* a) {
    std::lock_guard<std::mutex> lk(registry().mtx);
    registry().by_handle.emplace(a->handle(), a);
}

void remove(Actor* a) {
    std::lock_guard<std::mutex> lk(registry().mtx);
    registry().by_handle.erase(a->handle());
}

} // namespace detail

std::vector<RegistryEntry> snapshot() {
    std::lock_guard<std::mutex> lk(registry().mtx);
    std::vector<RegistryEntry> out;
    out.reserve(registry().by_handle.size());
    for (const auto& kv : registry().by_handle) {
        Actor* a = kv.second;
        RegistryEntry e;
        e.handle = a->handle();
        e.parent = a->parent() ? a->parent()->handle() : INVALID_HANDLE;
        e.name   = a->name();
        out.push_back(std::move(e));
    }
    return out;
}

// ---- Actor --------------------------------------------------------------

Actor::Actor(std::string name, Actor* parent)
    : name_(std::move(name)),
      handle_(detail::allocate_handle()),
      parent_(parent)
{
    if (parent_) parent_->children_.push_back(this);
    detail::add(this);
    bind::Engine::instance().actor_register(
        handle_, parent_ ? parent_->handle() : INVALID_HANDLE, name_);
}

Actor::~Actor() {
    // Detach from parent (best effort — subclass destructor order may
    // already have cleared the parent link in some edge cases).
    if (parent_) {
        auto& c = parent_->children_;
        c.erase(std::remove(c.begin(), c.end(), this), c.end());
    }
    // Unbind any variables we still own.
    for (bind::Handle h : owned_) bind::Engine::instance().unbind(h);
    owned_.clear();
    bind::Engine::instance().actor_unregister(handle_);
    detail::remove(this);
}

std::string Actor::qualified(const std::string& var_name) const {
    return name_ + "." + var_name;
}

bind::Handle Actor::bind_accessor(const std::string& var_name,
                                  bind::VarKind kind,
                                  std::function<bind::Value()> getter,
                                  std::function<void(const bind::Value&)> setter,
                                  bind::VarMeta meta)
{
    meta.actor_handle = handle_;
    const bind::Handle h = bind::Engine::instance().bind_accessor(
        qualified(var_name), kind, std::move(getter), std::move(setter),
        std::move(meta));
    if (h != bind::INVALID_HANDLE) owned_.push_back(h);
    return h;
}

} // namespace ergo::actor
