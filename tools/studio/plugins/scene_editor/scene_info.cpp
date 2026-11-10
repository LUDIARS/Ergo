#include "scene_info.h"

namespace ergo::studio::plugins::scene_editor {
namespace {

template <std::size_t N>
JsonValue vec(const std::array<double, N>& v) {
    JsonValue a = JsonValue::make_array();
    for (double x : v) a.push(jnum(x));
    return a;
}

JsonValue actor_json(const ActorSummary& a) {
    JsonValue overrides = JsonValue::make_array();
    for (const auto& o : a.overrides) overrides.push(jobj({{"path", jstr(o.path)}, {"value", jstr(o.value)}}));
    JsonValue vars = JsonValue::make_array();
    for (const auto& v : a.vars) vars.push(jobj({{"name", jstr(v.name)}, {"type", jstr(v.type)}, {"value", jstr(v.value)}}));
    JsonValue comps = JsonValue::make_array();
    for (const auto& c : a.components) {
        JsonValue props = JsonValue::make_array();
        for (const auto& [k, v] : c.props) {
            JsonValue pair = JsonValue::make_array();
            pair.push(jstr(k));
            pair.push(jstr(v));
            props.push(std::move(pair));
        }
        comps.push(jobj({{"type", jstr(c.type)}, {"props", props}}));
    }
    return jobj({
        {"id", jstr(a.id)},
        {"name", jstr(a.name)},
        {"type", jstr(a.type)},
        {"parent", jstr(a.parent)},
        {"transform", jobj({{"pos", vec(a.pos)}, {"rot", vec(a.rot)}, {"scale", vec(a.scale)}})},
        {"visual", jobj({{"kind", jstr(a.visual_kind)}, {"ref", jstr(a.visual_ref)}, {"material", jstr(a.visual_material)}})},
        {"instanceOf", jstr(a.instance_of)},
        {"overrides", overrides},
        {"vars", vars},
        {"components", comps},
    });
}

} // namespace

JsonValue to_json_value(const SceneInfo& s) {
    JsonValue actors = JsonValue::make_array();
    for (const auto& a : s.actors) actors.push(actor_json(a));
    return jobj({
        {"id", jstr(s.id)},
        {"domain", jstr(s.domain)},
        {"mount", jstr(s.mount)},
        {"actors", actors},
        {"camera", jobj({{"mode", jstr(s.camera_mode)}, {"target", vec(s.camera_target)},
                         {"distance", jnum(s.camera_distance)}, {"fov_deg", jnum(s.camera_fov_deg)}})},
        {"app", jstr(s.app)},
    });
}

} // namespace ergo::studio::plugins::scene_editor
