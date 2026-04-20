/// ergo_actor dummy plug — no-op Actor for link-only hosts.

#include "ergo/actor/actor.h"

namespace ergo::actor {

namespace detail {
Handle allocate_handle() { return 0; }
void   add    (Actor*)   {}
void   remove (Actor*)   {}
} // namespace detail

std::vector<RegistryEntry> snapshot() { return {}; }

Actor::Actor(std::string name, Actor* parent)
    : name_(std::move(name)), parent_(parent) {}
Actor::~Actor() = default;

std::string Actor::qualified(const std::string& v) const { return name_ + "." + v; }

bind::Handle Actor::bind_accessor(const std::string&, bind::VarKind,
                                  std::function<bind::Value()>,
                                  std::function<void(const bind::Value&)>,
                                  bind::VarMeta) {
    return bind::INVALID_HANDLE;
}

} // namespace ergo::actor
