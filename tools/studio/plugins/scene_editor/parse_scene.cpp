#include "parse_scene.h"

namespace ergo::studio::plugins::scene_editor {
namespace {

const JsonValue kEmptyObject = JsonValue::make_object();

const JsonValue& obj_or_empty(const JsonValue* v) { return v && v->is_object() ? *v : kEmptyObject; }

/// Text of a scalar field, `dflt` when absent / null. Objects, arrays and
/// numbers become their JSON text.
std::string str(const JsonValue* v, const std::string& dflt) {
    if (!v || v->is_null()) return dflt;
    if (v->is_string()) return v->s;
    return to_json(*v);
}

/// JSON text of a value; "" when absent.
std::string json_text(const JsonValue* v) { return v ? to_json(*v) : std::string(); }

template <std::size_t N>
bool as_vec(const JsonValue* v, std::array<double, N>& out) {
    if (!v || !v->is_array() || v->a->size() < N) return false;
    std::array<double, N> tmp{};
    for (std::size_t i = 0; i < N; ++i) {
        if (!(*v->a)[i].is_number()) return false;
        tmp[i] = (*v->a)[i].n;
    }
    out = tmp;
    return true;
}

ActorSummary parse_actor(const JsonValue& a) {
    ActorSummary s;
    s.id   = str(a.find("id"), "");
    s.name = str(a.find("name"), "");
    s.type = str(a.find("type"), "Actor");
    const JsonValue* parent = a.find("parent");
    s.parent = parent && parent->is_string() ? parent->s : std::string();

    const JsonValue& tr = obj_or_empty(a.find("transform"));
    as_vec(tr.find("pos"), s.pos);
    as_vec(tr.find("rot"), s.rot);
    as_vec(tr.find("scale"), s.scale);

    const JsonValue& vi = obj_or_empty(a.find("visual"));
    s.visual_kind     = str(vi.find("kind"), "");
    s.visual_ref      = str(vi.find("ref"), "");
    s.visual_material = str(vi.find("material"), "");
    s.instance_of     = str(a.find("instanceOf"), "");

    if (const JsonValue* ov = a.find("overrides"); ov && ov->is_array()) {
        for (const auto& o : *ov->a) {
            if (o.is_object()) s.overrides.push_back({str(o.find("path"), ""), json_text(o.find("value"))});
        }
    }
    if (const JsonValue* vars = a.find("vars"); vars && vars->is_array()) {
        for (const auto& v : *vars->a) {
            if (v.is_object()) s.vars.push_back({str(v.find("name"), ""), str(v.find("type"), ""), json_text(v.find("value"))});
        }
    }
    if (const JsonValue* comps = a.find("components"); comps && comps->is_array()) {
        for (const auto& c : *comps->a) {
            if (!c.is_object()) continue;
            ActorSummary::Component comp;
            comp.type = str(c.find("type"), "");
            for (const auto& [k, v] : *c.o) {
                if (k != "type") comp.props.emplace_back(k, to_json(v));
            }
            s.components.push_back(std::move(comp));
        }
    }
    return s;
}

} // namespace

std::optional<SceneInfo> parse_scene(const std::string& id, const std::string& json, const std::string& app) {
    JsonValue root;
    if (!ergo::common::jsonm::parse(json, root) || !root.is_object()) return std::nullopt;

    SceneInfo s;
    s.id     = id;
    s.domain = str(root.find("domain"), "level");
    s.mount  = str(root.find("mount"), "/level");
    s.app    = app;
    if (const JsonValue* actors = root.find("actors"); actors && actors->is_array()) {
        for (const auto& a : *actors->a) {
            if (a.is_object()) s.actors.push_back(parse_actor(a));
        }
    }
    const JsonValue& cam = obj_or_empty(root.find("camera"));
    s.camera_mode = str(cam.find("mode"), "orbit");
    as_vec(cam.find("target"), s.camera_target);
    if (const JsonValue* d = cam.find("distance"); d && d->is_number()) s.camera_distance = d->n;
    if (const JsonValue* f = cam.find("fov_deg"); f && f->is_number()) s.camera_fov_deg = f->n;
    return s;
}

} // namespace ergo::studio::plugins::scene_editor
