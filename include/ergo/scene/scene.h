#pragma once

/// ergo_scene - serializable look-dev scene document.
///
/// This first implementation intentionally owns the data model, CRUD, JSON
/// load/save, field-level patches, and camera matrices only. Render
/// submission is host-side. The live editor is the tools/ergo `scene_editor`
/// plugin, fed by the separate `ergo_scene_link` target (scene/editor_link.h).

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ergo::scene {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct Transform {
    Vec3 position{};
    Quat rotation{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct ResourceRef {
    std::string kind;
    std::string ref;
    std::string material;
};

/// Component attached to an actor (Renderable / Collider / Behavior / ...).
/// Extra properties beyond "type" are stored as (key, JSON-text) pairs so that
/// arbitrary component schemas round-trip without schema knowledge in ergo_scene.
struct ComponentSpec {
    std::string type;
    std::vector<std::pair<std::string, std::string>> props;
};

/// Typed public variable on an actor instance (writable via ergo_bind / editors).
/// value is JSON-encoded text so any JSON primitive or array/object is preserved.
struct VarSpec {
    std::string name;
    std::string type;   // "bool" | "int" | "float" | "string" | "vec3" | "color" | ...
    std::string value;  // JSON-encoded (e.g. "100", "3.14", "\"hello\"", "true")
};

/// Single prefab override: target property path → JSON-encoded replacement value.
struct OverrideEntry {
    std::string path;   // slash-separated field path, e.g. "vars/health"
    std::string value;  // JSON-encoded replacement value
};

struct ActorSpec {
    std::string id;
    std::string name;
    std::string type = "Actor";
    std::string parent;
    Transform transform{};
    ResourceRef visual{};
    std::vector<ComponentSpec> components;  // composition list (#240)
    std::vector<VarSpec> vars;              // public typed variables (#241)
    std::string instance_of;               // prefab reference (#242)
    std::vector<OverrideEntry> overrides;  // prefab property overrides (#242)
};

enum class CameraMode {
    Orbit,
    Fly,
};

struct Camera {
    CameraMode mode = CameraMode::Orbit;
    Vec3 target{};
    Vec3 position{0.0f, 0.0f, 8.0f};
    Vec3 front{0.0f, 0.0f, -1.0f};
    float distance = 8.0f;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float fov_deg = 50.0f;
    float z_near = 0.1f;
    float z_far = 1000.0f;

    void view_matrix(float out[16]) const;
    void projection_matrix(float aspect, float out[16]) const;
};

struct BloomSettings {
    bool enabled = false;
    float threshold = 1.0f;
    float intensity = 0.0f;
};

struct PostProcess {
    std::string tonemap = "none";
    float exposure = 1.0f;
    BloomSettings bloom{};
};

class Scene {
public:
    Scene() = default;

    static std::unique_ptr<Scene> load_json(std::string_view json);
    static std::unique_ptr<Scene> load_file(const std::string& path);

    bool load(const std::string& path);
    bool save(const std::string& path) const;

    std::string to_json() const;

    const std::string& id() const { return id_; }
    void set_id(std::string id) { id_ = std::move(id); }

    const std::string& domain() const { return domain_; }
    void set_domain(std::string domain) { domain_ = std::move(domain); }

    const std::string& mount() const { return mount_; }
    void set_mount(std::string mount) { mount_ = std::move(mount); }

    const std::vector<std::string>& imports() const { return imports_; }
    void add_import(std::string path);
    void clear_imports();

    const Camera& camera() const { return camera_; }
    Camera& camera() { return camera_; }

    const PostProcess& post_process() const { return post_process_; }
    PostProcess& post_process() { return post_process_; }

    const std::vector<ActorSpec>& actors() const { return actors_; }
    std::size_t actor_count() const { return actors_.size(); }

    ActorSpec* find_actor(const std::string& id);
    const ActorSpec* find_actor(const std::string& id) const;

    bool add_actor(ActorSpec actor);
    bool remove_actor(const std::string& id);
    void clear_actors();

    /// Apply one field-level edit (the scene_editor wire format) to an actor.
    /// `value_json` is the JSON text of the new value. Supported fields:
    ///   name, type, visual.kind, visual.ref, visual.material  — JSON string
    ///   transform.pos, transform.scale                        — [x, y, z]
    ///   transform.rot          — [x, y, z, w], stored normalized
    ///   vars/<name>/value      — any JSON, stored serialized (var must exist)
    ///   parent                 — actor id in this scene, or "" / null to
    ///                            clear; not the actor itself or a descendant
    /// Returns false (and changes nothing) for an unknown actor or field, a
    /// value of the wrong JSON type or arity, or malformed JSON. The legacy
    /// "transform.rot.xyz" path is rejected: rotations travel as quaternions.
    bool apply_patch(const std::string& actor_id, std::string_view field, std::string_view value_json);

private:
    std::string id_;
    std::string domain_ = "level";
    std::string mount_ = "/level";
    std::vector<std::string> imports_;
    Camera camera_{};
    PostProcess post_process_{};
    std::vector<ActorSpec> actors_;
};

const char* to_string(CameraMode mode);
CameraMode camera_mode_from_string(const std::string& value);

/// Runtime multi-scene compositor: mount / unmount Scene documents and present
/// a unified actor graph. Scenes are keyed by their id (from Scene::id()).
/// Actors across all mounted scenes are visited in mount order.
class SceneGraph {
public:
    SceneGraph() = default;

    /// Mount a scene. Returns false if a scene with the same id is already mounted.
    bool mount(std::unique_ptr<Scene> scene);

    /// Unmount (and destroy) the scene with the given id. Returns false if not found.
    bool unmount(const std::string& scene_id);

    /// True if a scene with this id is currently mounted.
    bool is_mounted(const std::string& scene_id) const;

    /// The mounted scene with this id, or nullptr.
    Scene* find_scene(const std::string& scene_id);
    const Scene* find_scene(const std::string& scene_id) const;

    std::size_t scene_count() const { return scenes_.size(); }

    /// Find an actor by id across all mounted scenes (first match wins).
    const ActorSpec* find_actor(const std::string& actor_id) const;

    /// Call visitor(scene, actor) for every actor in every mounted scene.
    void for_each_actor(const std::function<void(const Scene&, const ActorSpec&)>& visitor) const;

private:
    std::vector<std::unique_ptr<Scene>> scenes_;
};

void mat4_identity(float out[16]);

} // namespace ergo::scene
