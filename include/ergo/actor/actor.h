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
///
/// ## Per-actor time scale
///
/// Each actor carries a `time_scale` multiplier (default 1.0).  Call
/// `scaled_dt(dt)` in your tick to get the actor-local delta time:
///
///     void MyActor::tick(float world_dt) {
///         const float dt = scaled_dt(world_dt);   // 0 if actor is paused
///         anim_.update(dt);
///         effect_.update(dt);
///     }
///
/// The base dt passed in can be either the world dt (from
/// `ergo::world_time::Engine::update`) so that global hit-stop still
/// applies, or real dt if the actor should be immune to world effects.
/// The variable editor exposes `{actor_name}.time_scale` for live tuning.

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

    // ---- Per-actor time scale --------------------------------------------

    /// Local time-scale multiplier. Default 1.0. Clamped to [0, ∞).
    /// Setting to 0 pauses this actor's animations and effects without
    /// affecting any other actor or the global world time.
    float time_scale() const { return time_scale_; }

    /// Set the local time-scale. Values < 0 are clamped to 0.
    void set_time_scale(float s);

    /// Multiply `dt` by this actor's time_scale and return the result.
    /// Pass the world dt (from WorldTime) or real dt depending on
    /// whether the actor should honour global time effects.
    float scaled_dt(float dt) const { return dt * time_scale_; }

    // -----------------------------------------------------------------------

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
    Handle                     handle_     = INVALID_HANDLE;
    Actor*                     parent_     = nullptr;
    std::vector<Actor*>        children_;
    std::vector<bind::Handle>  owned_;
    float                      time_scale_ = 1.0f;
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
