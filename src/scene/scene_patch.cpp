// Scene::apply_patch — field-level edits in the scene_editor wire format.
//
// Kept apart from scene.cpp (document load/save) because it is the editing
// surface: every path accepted here is one the tools/ergo `scene_editor`
// plugin can send, and is documented in spec/feature/tool/scene_editor.md.

#include "ergo/scene/scene.h"

#include "ergo/common/json_min.h"

#include <cmath>
#include <string>

namespace ergo::scene {
namespace {

using ergo::common::jsonm::JsonValue;

constexpr std::string_view kVarsPrefix = "vars/";
constexpr std::string_view kVarsSuffix = "/value";

/// Exactly `n` finite numbers.
bool read_numbers(const JsonValue& v, std::size_t n, float* out) {
    if (!v.is_array() || !v.a || v.a->size() != n) return false;
    for (std::size_t i = 0; i < n; ++i) {
        const JsonValue& e = (*v.a)[i];
        if (!e.is_number() || !std::isfinite(e.n)) return false;
        out[i] = static_cast<float>(e.n);
    }
    return true;
}

bool patch_string(const JsonValue& v, std::string& field) {
    if (!v.is_string()) return false;
    field = v.s;
    return true;
}

bool patch_vec3(const JsonValue& v, Vec3& field) {
    float f[3];
    if (!read_numbers(v, 3, f)) return false;
    field = Vec3{f[0], f[1], f[2]};
    return true;
}

bool patch_quat(const JsonValue& v, Quat& field) {
    float f[4];
    if (!read_numbers(v, 4, f)) return false;
    const double len = std::sqrt(double(f[0]) * f[0] + double(f[1]) * f[1] +
                                 double(f[2]) * f[2] + double(f[3]) * f[3]);
    if (!(len > 1e-6)) return false;  // zero quaternion has no rotation
    field = Quat{float(f[0] / len), float(f[1] / len), float(f[2] / len), float(f[3] / len)};
    return true;
}

} // namespace

bool Scene::apply_patch(const std::string& actor_id, std::string_view field, std::string_view value_json) {
    ActorSpec* actor = find_actor(actor_id);
    if (!actor) return false;

    JsonValue v;
    if (!ergo::common::jsonm::parse(std::string(value_json), v)) return false;

    if (field == "name")            return patch_string(v, actor->name);
    if (field == "type")            return patch_string(v, actor->type);
    if (field == "visual.kind")     return patch_string(v, actor->visual.kind);
    if (field == "visual.ref")      return patch_string(v, actor->visual.ref);
    if (field == "visual.material") return patch_string(v, actor->visual.material);
    if (field == "transform.pos")   return patch_vec3(v, actor->transform.position);
    if (field == "transform.scale") return patch_vec3(v, actor->transform.scale);
    if (field == "transform.rot")   return patch_quat(v, actor->transform.rotation);

    if (field == "parent") {
        // null mirrors the file format's "no parent".
        if (v.is_null() || (v.is_string() && v.s.empty())) {
            actor->parent.clear();
            return true;
        }
        if (!v.is_string() || v.s == actor_id || !find_actor(v.s)) return false;
        // Walk up from the new parent: reaching the actor means a cycle.
        // Bounded by the actor count so a pre-existing loop cannot hang us.
        std::string cur = v.s;
        for (std::size_t steps = 0; !cur.empty() && steps <= actors_.size(); ++steps) {
            if (cur == actor_id) return false;
            const ActorSpec* up = find_actor(cur);
            cur = up ? up->parent : std::string();
        }
        actor->parent = v.s;
        return true;
    }

    if (field.size() > kVarsPrefix.size() + kVarsSuffix.size() &&
        field.compare(0, kVarsPrefix.size(), kVarsPrefix) == 0 &&
        field.compare(field.size() - kVarsSuffix.size(), kVarsSuffix.size(), kVarsSuffix) == 0) {
        const std::string_view name =
            field.substr(kVarsPrefix.size(), field.size() - kVarsPrefix.size() - kVarsSuffix.size());
        for (VarSpec& var : actor->vars) {
            if (var.name != name) continue;
            var.value = ergo::common::jsonm::serialize(v);
            return true;
        }
        return false;
    }

    // Unknown path, including the legacy "transform.rot.xyz".
    return false;
}

} // namespace ergo::scene
