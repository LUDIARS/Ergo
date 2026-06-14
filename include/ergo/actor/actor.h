#pragma once

/// Actor — scene-graph binding base class.
///
/// Subclasses inherit from Actor to publish themselves (and their
/// variables) to the variable editor's tree view. Variables bound via
/// `bind_var()` / `bind_accessor()` are tagged with the actor's handle
/// so the UI can group them under the right node.
///
/// Thin by design: this module owns the tree topology and forwards all
/// variable publication to `ergo_bind`. It does not define transform,
/// visibility, or any other game-engine concepts — those are a host
/// concern. What Actor gives you is stable identity (a handle) and a
/// consistent place to hang live-tuned variables.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Depend only on the lightweight bind value types (Handle / VarKind / VarMeta
// / Value), NOT on ergo/bind/bind.h. The actual Engine call lives out-of-line
// in actor.cpp so this public header stays decoupled from the WS Engine.
#include "ergo/bind/types.h"

namespace ergo::actor {

using Handle = uint64_t;
constexpr Handle INVALID_HANDLE = 0;

class Actor {
public:
    /// `name` is the UI display label; `parent` may be nullptr for root
    /// actors. The actor auto-registers with the process-wide registry
    /// and notifies `ergo::bind::Engine` immediately.
    explicit Actor(std::string name, Actor* parent = nullptr);
    virtual ~Actor();

    Actor(const Actor&)            = delete;
    Actor& operator=(const Actor&) = delete;

    const std::string&         name()     const { return name_; }
    Handle                     handle()   const { return handle_; }
    Actor*                     parent()   const { return parent_; }
    const std::vector<Actor*>& children() const { return children_; }

    /// Compose the wire name for a variable bound under this actor.
    /// Default: `"{actor_name}.{var_name}"` — subclasses may override
    /// (e.g. to use slashes for a deeper hierarchy).
    virtual std::string qualified(const std::string& var_name) const;

protected:
    /// Bind a typed lvalue under this actor. Supported T: bool, int32_t,
    /// int64_t, float, double, std::string (matches ergo::bind::Engine::bind<T>).
    /// Defined out-of-line in actor.cpp with explicit instantiations for the
    /// supported types, so this header need not include the bind Engine.
    template<class T>
    bind::Handle bind_var(const std::string& var_name, T* ptr,
                          bind::VarMeta meta = {});

    /// Bind a custom getter / setter pair under this actor.
    bind::Handle bind_accessor(const std::string& var_name,
                               bind::VarKind kind,
                               std::function<bind::Value()> getter,
                               std::function<void(const bind::Value&)> setter,
                               bind::VarMeta meta = {});

private:
    std::string                name_;
    Handle                     handle_ = INVALID_HANDLE;
    Actor*                     parent_ = nullptr;
    std::vector<Actor*>        children_;
    std::vector<bind::Handle>  owned_;
};

// -------------------------------------------------------------------------
// Process-wide registry helpers (internal — hosts normally don't call
// these directly, but ergo_bind uses `snapshot()` on reconnect to replay
// every known actor's `actor_register` message).
// -------------------------------------------------------------------------

struct RegistryEntry {
    Handle       handle;
    Handle       parent;     // INVALID_HANDLE for root
    std::string  name;
};

namespace detail {
    Handle      allocate_handle();
    void        add    (Actor*);
    void        remove (Actor*);
}

std::vector<RegistryEntry> snapshot();

} // namespace ergo::actor
