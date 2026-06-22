#pragma once

/// Scene — lightweight, non-owning bundler for `ergo::actor::Actor`.
///
/// A `cast::Scene` groups existing Actor instances under a name so a host
/// can flip them on/off in bulk and capture/restore their activation state.
/// It is RENDER-INDEPENDENT: no Pictor, no ergo_render, no rendering headers.
/// It is purely a logical grouping + activation + snapshot utility.
///
/// Non-owning by design: a Scene holds raw `Actor*` and never deletes them.
/// The host owns actor lifetime; membership is keyed by the actor's stable
/// `handle()`. Single responsibility — grouping / activation / snapshot only.
/// No I/O, no JSON, no rendering. `SceneSnapshot` is a plain data struct; a
/// consumer may serialize it however it likes.
///
/// Relationship to `ergo_scene`: distinct modules. `ergo_scene` is a heavier
/// GUI-editable look-dev *document* (camera + actor specs + post-process,
/// serialized to `*.scene.json`, depends on `ergo_render`). `ergo_cast` is a
/// thin runtime cast list with zero rendering coupling.

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "ergo/actor/actor.h"

namespace ergo::cast {

using Actor  = ergo::actor::Actor;
using Handle = ergo::actor::Handle;

/// One member's captured state in a snapshot.
struct ActorEntry {
    Handle      handle;
    std::string name;
    bool        active;
};

/// Plain data capture of a Scene's state. Serialize externally if needed.
struct SceneSnapshot {
    std::string             scene_name;
    bool                    scene_active = false;
    std::vector<ActorEntry> entries;
};

/// Non-owning group of Actors with bulk + per-actor activation and snapshot.
class Scene {
public:
    explicit Scene(std::string name);

    const std::string& name() const { return name_; }

    // --- Membership -------------------------------------------------------
    /// Add `a` to the scene. No-op if `a` is null or already a member
    /// (matched by handle). New members start with the given per-actor flag.
    void                add(Actor* a, bool active = true);
    bool                remove(Actor* a);
    bool                remove(Handle h);
    bool                contains(const Actor* a) const;
    bool                contains(Handle h) const;
    std::size_t         size() const { return entries_.size(); }
    /// All member pointers in insertion order.
    std::vector<Actor*> actors() const;

    // --- Scene-level activation ------------------------------------------
    /// Activate the whole scene. Fires on_activate for each member whose
    /// per-actor flag is active (effective state flips false -> true).
    void activate();
    /// Deactivate the whole scene. Fires on_deactivate for each member that
    /// is currently effectively-active (effective state flips true -> false).
    void deactivate();
    bool active() const { return scene_active_; }

    // --- Per-actor activation --------------------------------------------
    /// Set a member's per-actor flag. Fires the matching callback only if the
    /// effective state (scene_active_ && entry.active) changes. No-op if `a`
    /// is not a member.
    void activate(Actor* a);
    void deactivate(Actor* a);
    /// Effective state = scene_active_ && entry.active. False if not a member.
    bool is_active(const Actor* a) const;

    // --- Lifecycle callbacks (render-independent) ------------------------
    void on_activate(std::function<void(Actor*)> cb);
    void on_deactivate(std::function<void(Actor*)> cb);

    // --- Snapshot / restore ----------------------------------------------
    /// Capture scene name, scene_active, and each member's {handle,name,active}.
    SceneSnapshot snapshot() const;
    /// Restore scene_active_ and, for entries whose handle matches a current
    /// member, that member's per-actor flag. Unknown handles are ignored;
    /// membership is not changed. Fires callbacks for members whose effective
    /// state changed.
    void restore(const SceneSnapshot& snap);

private:
    struct Entry {
        Actor* actor;
        bool   active;
    };

    std::vector<Entry>::iterator       find(Handle h);
    std::vector<Entry>::const_iterator find(Handle h) const;

    void fire_activate(Actor* a) const;
    void fire_deactivate(Actor* a) const;

    std::string                              name_;
    bool                                     scene_active_ = false;
    std::vector<Entry>                       entries_;
    std::vector<std::function<void(Actor*)>> on_activate_;
    std::vector<std::function<void(Actor*)>> on_deactivate_;
};

} // namespace ergo::cast
