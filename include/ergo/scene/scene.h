#pragma once

/// ergo_scene - serializable look-dev scene document.
///
/// This first implementation intentionally owns the data model, CRUD, JSON
/// load/save, and camera matrices only. Render submission and editor UI can
/// build on this stable document layer later.

#include <cstddef>
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

struct ActorSpec {
    std::string id;
    std::string name;
    std::string type = "Actor";
    std::string parent;
    Transform transform{};
    ResourceRef visual{};
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

void mat4_identity(float out[16]);

} // namespace ergo::scene
