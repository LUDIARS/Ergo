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

template<class T>
bind::Handle Actor::bind_var(const std::string&, T*, bind::VarMeta) {
    return bind::INVALID_HANDLE;
}
template bind::Handle Actor::bind_var<bool>       (const std::string&, bool*,        bind::VarMeta);
template bind::Handle Actor::bind_var<int32_t>    (const std::string&, int32_t*,     bind::VarMeta);
template bind::Handle Actor::bind_var<int64_t>    (const std::string&, int64_t*,     bind::VarMeta);
template bind::Handle Actor::bind_var<float>      (const std::string&, float*,       bind::VarMeta);
template bind::Handle Actor::bind_var<double>     (const std::string&, double*,      bind::VarMeta);
template bind::Handle Actor::bind_var<std::string>(const std::string&, std::string*, bind::VarMeta);

} // namespace ergo::actor
