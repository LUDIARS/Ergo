#include "ergo/scene/scene.h"

#include "ergo/common/json_min.h"

#include <cmath>
#include <fstream>
#include <iterator>
#include <utility>

namespace ergo::scene {
namespace {

using ergo::common::jsonm::JsonValue;

constexpr float kPi = 3.14159265358979323846f;

float deg_to_rad(float deg) {
    return deg * (kPi / 180.0f);
}

Vec3 add(Vec3 a, Vec3 b) {
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 sub(Vec3 a, Vec3 b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 scale_vec(Vec3 v, float s) {
    return Vec3{v.x * s, v.y * s, v.z * s};
}

float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(Vec3 a, Vec3 b) {
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

Vec3 normalize(Vec3 v) {
    const float len = std::sqrt(dot(v, v));
    if (len <= 0.000001f) return Vec3{};
    return scale_vec(v, 1.0f / len);
}

float number_or(const JsonValue* v, float fallback) {
    return v && v->is_number() ? static_cast<float>(v->n) : fallback;
}

std::string string_or(const JsonValue* v, std::string fallback = {}) {
    return v && v->is_string() ? v->s : std::move(fallback);
}

bool bool_or(const JsonValue* v, bool fallback) {
    return v && v->is_bool() ? v->b : fallback;
}

Vec3 read_vec3(const JsonValue* v, Vec3 fallback) {
    if (!v || !v->is_array() || !v->a || v->a->size() < 3) return fallback;
    return Vec3{
        number_or(&(*v->a)[0], fallback.x),
        number_or(&(*v->a)[1], fallback.y),
        number_or(&(*v->a)[2], fallback.z),
    };
}

Quat read_quat(const JsonValue* v, Quat fallback) {
    if (!v || !v->is_array() || !v->a || v->a->size() < 4) return fallback;
    return Quat{
        number_or(&(*v->a)[0], fallback.x),
        number_or(&(*v->a)[1], fallback.y),
        number_or(&(*v->a)[2], fallback.z),
        number_or(&(*v->a)[3], fallback.w),
    };
}

JsonValue vec3_json(Vec3 v) {
    auto a = JsonValue::make_array();
    a.push(JsonValue::make_number(v.x));
    a.push(JsonValue::make_number(v.y));
    a.push(JsonValue::make_number(v.z));
    return a;
}

JsonValue quat_json(Quat q) {
    auto a = JsonValue::make_array();
    a.push(JsonValue::make_number(q.x));
    a.push(JsonValue::make_number(q.y));
    a.push(JsonValue::make_number(q.z));
    a.push(JsonValue::make_number(q.w));
    return a;
}

JsonValue transform_json(const Transform& t) {
    auto obj = JsonValue::make_object();
    obj.set("pos", vec3_json(t.position));
    obj.set("rot", quat_json(t.rotation));
    obj.set("scale", vec3_json(t.scale));
    return obj;
}

Transform read_transform(const JsonValue* v) {
    Transform t;
    if (!v || !v->is_object()) return t;
    t.position = read_vec3(v->find("pos"), t.position);
    t.rotation = read_quat(v->find("rot"), t.rotation);
    t.scale = read_vec3(v->find("scale"), t.scale);
    return t;
}

ResourceRef read_visual(const JsonValue* v) {
    ResourceRef r;
    if (!v || !v->is_object()) return r;
    r.kind = string_or(v->find("kind"));
    r.ref = string_or(v->find("ref"));
    r.material = string_or(v->find("material"));
    return r;
}

JsonValue visual_json(const ResourceRef& r) {
    auto obj = JsonValue::make_object();
    obj.set("kind", JsonValue::make_string(r.kind));
    obj.set("ref", JsonValue::make_string(r.ref));
    obj.set("material", JsonValue::make_string(r.material));
    return obj;
}

bool read_file_text(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

bool write_file_text(const std::string& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(file);
}

} // namespace

const char* to_string(CameraMode mode) {
    switch (mode) {
        case CameraMode::Orbit: return "orbit";
        case CameraMode::Fly: return "fly";
    }
    return "orbit";
}

CameraMode camera_mode_from_string(const std::string& value) {
    return value == "fly" ? CameraMode::Fly : CameraMode::Orbit;
}

void mat4_identity(float out[16]) {
    for (int i = 0; i < 16; ++i) out[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}

void Camera::view_matrix(float out[16]) const {
    const Vec3 up{0.0f, 1.0f, 0.0f};
    Vec3 eye = position;
    Vec3 center = add(position, front);
    if (mode == CameraMode::Orbit) {
        const float cp = std::cos(pitch);
        const Vec3 dir{
            std::sin(yaw) * cp,
            std::sin(pitch),
            std::cos(yaw) * cp,
        };
        eye = add(target, scale_vec(dir, distance));
        center = target;
    }

    const Vec3 f = normalize(sub(center, eye));
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);

    mat4_identity(out);
    out[0] = s.x;  out[4] = s.y;  out[8]  = s.z;
    out[1] = u.x;  out[5] = u.y;  out[9]  = u.z;
    out[2] = -f.x; out[6] = -f.y; out[10] = -f.z;
    out[12] = -dot(s, eye);
    out[13] = -dot(u, eye);
    out[14] = dot(f, eye);
}

void Camera::projection_matrix(float aspect, float out[16]) const {
    if (aspect <= 0.0f) aspect = 1.0f;
    const float f = 1.0f / std::tan(deg_to_rad(fov_deg) * 0.5f);
    for (int i = 0; i < 16; ++i) out[i] = 0.0f;
    out[0] = f / aspect;
    out[5] = -f;
    out[10] = z_far / (z_near - z_far);
    out[11] = -1.0f;
    out[14] = (z_near * z_far) / (z_near - z_far);
}

std::unique_ptr<Scene> Scene::load_json(std::string_view json) {
    JsonValue root;
    if (!ergo::common::jsonm::parse(std::string(json), root) || !root.is_object()) return nullptr;

    auto scene = std::make_unique<Scene>();

    if (const JsonValue* cam = root.find("camera"); cam && cam->is_object()) {
        scene->camera_.mode = camera_mode_from_string(string_or(cam->find("mode"), "orbit"));
        scene->camera_.target = read_vec3(cam->find("target"), scene->camera_.target);
        scene->camera_.position = read_vec3(cam->find("position"), scene->camera_.position);
        scene->camera_.front = read_vec3(cam->find("front"), scene->camera_.front);
        scene->camera_.distance = number_or(cam->find("distance"), scene->camera_.distance);
        scene->camera_.yaw = number_or(cam->find("yaw"), scene->camera_.yaw);
        scene->camera_.pitch = number_or(cam->find("pitch"), scene->camera_.pitch);
        scene->camera_.fov_deg = number_or(cam->find("fov_deg"), scene->camera_.fov_deg);
        scene->camera_.z_near = number_or(cam->find("z_near"), scene->camera_.z_near);
        scene->camera_.z_far = number_or(cam->find("z_far"), scene->camera_.z_far);
    }

    if (const JsonValue* pp = root.find("post_process"); pp && pp->is_object()) {
        scene->post_process_.tonemap = string_or(pp->find("tonemap"), scene->post_process_.tonemap);
        scene->post_process_.exposure = number_or(pp->find("exposure"), scene->post_process_.exposure);
        if (const JsonValue* bloom = pp->find("bloom"); bloom && bloom->is_object()) {
            scene->post_process_.bloom.enabled = bool_or(bloom->find("enabled"), scene->post_process_.bloom.enabled);
            scene->post_process_.bloom.threshold = number_or(bloom->find("threshold"), scene->post_process_.bloom.threshold);
            scene->post_process_.bloom.intensity = number_or(bloom->find("intensity"), scene->post_process_.bloom.intensity);
        }
    }

    scene->id_ = string_or(root.find("id"));
    scene->domain_ = string_or(root.find("domain"), scene->domain_);
    scene->mount_ = string_or(root.find("mount"), scene->mount_);
    if (const JsonValue* imports = root.find("imports"); imports && imports->is_array() && imports->a) {
        for (const JsonValue& item : *imports->a) {
            if (item.is_string() && !item.s.empty()) scene->add_import(item.s);
        }
    }

    const JsonValue* actors = root.find("actors");
    if (!actors) actors = root.find("objects");
    if (actors && actors->is_array() && actors->a) {
        for (const JsonValue& item : *actors->a) {
            if (!item.is_object()) continue;
            ActorSpec actor;
            actor.id = string_or(item.find("id"));
            if (actor.id.empty()) continue;
            actor.name = string_or(item.find("name"));
            actor.type = string_or(item.find("type"), actor.type);
            if (const JsonValue* parent = item.find("parent"); parent && !parent->is_null()) {
                actor.parent = string_or(parent);
            }
            actor.transform = read_transform(item.find("transform"));
            actor.visual = read_visual(item.find("visual"));
            scene->add_actor(std::move(actor));
        }
    }

    return scene;
}

std::unique_ptr<Scene> Scene::load_file(const std::string& path) {
    std::string text;
    if (!read_file_text(path, text)) return nullptr;
    return load_json(text);
}

bool Scene::load(const std::string& path) {
    auto loaded = load_file(path);
    if (!loaded) return false;
    *this = std::move(*loaded);
    return true;
}

bool Scene::save(const std::string& path) const {
    return write_file_text(path, to_json());
}

std::string Scene::to_json() const {
    auto root = JsonValue::make_object();
    root.set("version", JsonValue::make_number(1));
    root.set("id", JsonValue::make_string(id_));
    root.set("domain", JsonValue::make_string(domain_));
    root.set("mount", JsonValue::make_string(mount_));

    auto imports = JsonValue::make_array();
    for (const auto& path : imports_) {
        imports.push(JsonValue::make_string(path));
    }
    root.set("imports", std::move(imports));

    auto cam = JsonValue::make_object();
    cam.set("mode", JsonValue::make_string(to_string(camera_.mode)));
    cam.set("target", vec3_json(camera_.target));
    cam.set("position", vec3_json(camera_.position));
    cam.set("front", vec3_json(camera_.front));
    cam.set("distance", JsonValue::make_number(camera_.distance));
    cam.set("yaw", JsonValue::make_number(camera_.yaw));
    cam.set("pitch", JsonValue::make_number(camera_.pitch));
    cam.set("fov_deg", JsonValue::make_number(camera_.fov_deg));
    cam.set("z_near", JsonValue::make_number(camera_.z_near));
    cam.set("z_far", JsonValue::make_number(camera_.z_far));
    root.set("camera", std::move(cam));

    auto bloom = JsonValue::make_object();
    bloom.set("enabled", JsonValue::make_bool(post_process_.bloom.enabled));
    bloom.set("threshold", JsonValue::make_number(post_process_.bloom.threshold));
    bloom.set("intensity", JsonValue::make_number(post_process_.bloom.intensity));

    auto pp = JsonValue::make_object();
    pp.set("tonemap", JsonValue::make_string(post_process_.tonemap));
    pp.set("exposure", JsonValue::make_number(post_process_.exposure));
    pp.set("bloom", std::move(bloom));
    root.set("post_process", std::move(pp));

    auto actors = JsonValue::make_array();
    for (const auto& actor : actors_) {
        auto obj = JsonValue::make_object();
        obj.set("id", JsonValue::make_string(actor.id));
        if (!actor.name.empty()) obj.set("name", JsonValue::make_string(actor.name));
        obj.set("type", JsonValue::make_string(actor.type));
        if (actor.parent.empty()) {
            obj.set("parent", JsonValue::make_null());
        } else {
            obj.set("parent", JsonValue::make_string(actor.parent));
        }
        obj.set("transform", transform_json(actor.transform));
        obj.set("visual", visual_json(actor.visual));
        actors.push(std::move(obj));
    }
    root.set("actors", std::move(actors));

    return ergo::common::jsonm::serialize(root);
}

void Scene::add_import(std::string path) {
    if (!path.empty()) imports_.push_back(std::move(path));
}

void Scene::clear_imports() {
    imports_.clear();
}

ActorSpec* Scene::find_actor(const std::string& id) {
    for (auto& actor : actors_) {
        if (actor.id == id) return &actor;
    }
    return nullptr;
}

const ActorSpec* Scene::find_actor(const std::string& id) const {
    for (const auto& actor : actors_) {
        if (actor.id == id) return &actor;
    }
    return nullptr;
}

bool Scene::add_actor(ActorSpec actor) {
    if (actor.id.empty() || find_actor(actor.id)) return false;
    actors_.push_back(std::move(actor));
    return true;
}

bool Scene::remove_actor(const std::string& id) {
    for (auto it = actors_.begin(); it != actors_.end(); ++it) {
        if (it->id != id) continue;
        actors_.erase(it);
        for (auto& actor : actors_) {
            if (actor.parent == id) actor.parent.clear();
        }
        return true;
    }
    return false;
}

void Scene::clear_actors() {
    actors_.clear();
}

} // namespace ergo::scene
