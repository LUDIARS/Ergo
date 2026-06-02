#include "ergo/cast/scene.h"

#include <algorithm>
#include <utility>

namespace ergo::cast {

Scene::Scene(std::string name) : name_(std::move(name)) {}

// ---------------------------------------------------------------------------
// Lookup helpers — membership is keyed by the actor's stable handle.
// ---------------------------------------------------------------------------

std::vector<Scene::Entry>::iterator Scene::find(Handle h) {
    return std::find_if(entries_.begin(), entries_.end(),
                        [h](const Entry& e) { return e.actor->handle() == h; });
}

std::vector<Scene::Entry>::const_iterator Scene::find(Handle h) const {
    return std::find_if(entries_.begin(), entries_.end(),
                        [h](const Entry& e) { return e.actor->handle() == h; });
}

void Scene::fire_activate(Actor* a) const {
    for (const auto& cb : on_activate_)
        if (cb) cb(a);
}

void Scene::fire_deactivate(Actor* a) const {
    for (const auto& cb : on_deactivate_)
        if (cb) cb(a);
}

// ---------------------------------------------------------------------------
// Membership
// ---------------------------------------------------------------------------

void Scene::add(Actor* a, bool active) {
    if (a == nullptr) return;
    if (find(a->handle()) != entries_.end()) return;  // already a member
    entries_.push_back(Entry{a, active});
}

bool Scene::remove(Actor* a) {
    if (a == nullptr) return false;
    return remove(a->handle());
}

bool Scene::remove(Handle h) {
    const auto it = find(h);
    if (it == entries_.end()) return false;
    entries_.erase(it);
    return true;
}

bool Scene::contains(const Actor* a) const {
    if (a == nullptr) return false;
    return find(a->handle()) != entries_.end();
}

bool Scene::contains(Handle h) const {
    return find(h) != entries_.end();
}

std::vector<Actor*> Scene::actors() const {
    std::vector<Actor*> out;
    out.reserve(entries_.size());
    for (const auto& e : entries_) out.push_back(e.actor);
    return out;
}

// ---------------------------------------------------------------------------
// Scene-level activation. Effective state of a member = scene_active_ &&
// entry.active; callbacks fire only on effective-state transitions.
// ---------------------------------------------------------------------------

void Scene::activate() {
    if (scene_active_) return;          // no transition
    scene_active_ = true;
    for (const auto& e : entries_)
        if (e.active) fire_activate(e.actor);  // false -> true
}

void Scene::deactivate() {
    if (!scene_active_) return;         // no transition
    scene_active_ = false;
    for (const auto& e : entries_)
        if (e.active) fire_deactivate(e.actor);  // true -> false
}

// ---------------------------------------------------------------------------
// Per-actor activation
// ---------------------------------------------------------------------------

void Scene::activate(Actor* a) {
    if (a == nullptr) return;
    const auto it = find(a->handle());
    if (it == entries_.end()) return;       // not a member -> no-op
    if (it->active) return;                 // per-actor flag unchanged
    it->active = true;
    if (scene_active_) fire_activate(it->actor);  // effective false -> true
}

void Scene::deactivate(Actor* a) {
    if (a == nullptr) return;
    const auto it = find(a->handle());
    if (it == entries_.end()) return;       // not a member -> no-op
    if (!it->active) return;                // per-actor flag unchanged
    it->active = false;
    if (scene_active_) fire_deactivate(it->actor);  // effective true -> false
}

bool Scene::is_active(const Actor* a) const {
    if (a == nullptr) return false;
    const auto it = find(a->handle());
    if (it == entries_.end()) return false;
    return scene_active_ && it->active;
}

// ---------------------------------------------------------------------------
// Lifecycle callbacks — multiple registrations accumulate.
// ---------------------------------------------------------------------------

void Scene::on_activate(std::function<void(Actor*)> cb) {
    on_activate_.push_back(std::move(cb));
}

void Scene::on_deactivate(std::function<void(Actor*)> cb) {
    on_deactivate_.push_back(std::move(cb));
}

// ---------------------------------------------------------------------------
// Snapshot / restore
// ---------------------------------------------------------------------------

SceneSnapshot Scene::snapshot() const {
    SceneSnapshot snap;
    snap.scene_name   = name_;
    snap.scene_active = scene_active_;
    snap.entries.reserve(entries_.size());
    for (const auto& e : entries_)
        snap.entries.push_back(ActorEntry{e.actor->handle(), e.actor->name(), e.active});
    return snap;
}

void Scene::restore(const SceneSnapshot& snap) {
    // Effective state of every current member, captured before any mutation,
    // so we can fire callbacks for the members whose effective state changes.
    const bool was_scene_active = scene_active_;

    struct Before { Actor* actor; bool effective; };
    std::vector<Before> before;
    before.reserve(entries_.size());
    for (const auto& e : entries_)
        before.push_back(Before{e.actor, was_scene_active && e.active});

    // Apply scene-level flag.
    scene_active_ = snap.scene_active;

    // Apply per-actor flags for entries whose handle matches a member.
    // Unknown handles are ignored; membership is never changed here.
    for (const auto& se : snap.entries) {
        const auto it = find(se.handle);
        if (it != entries_.end()) it->active = se.active;
    }

    // Fire callbacks for members whose effective state flipped.
    for (const auto& b : before) {
        const auto it = find(b.actor->handle());
        if (it == entries_.end()) continue;  // (cannot happen; members are stable)
        const bool now = scene_active_ && it->active;
        if (now == b.effective) continue;
        if (now) fire_activate(it->actor);
        else     fire_deactivate(it->actor);
    }
}

} // namespace ergo::cast
