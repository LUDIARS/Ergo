#pragma once
/// SceneInfo snapshot shown by the scene editor (port of scene_editor/protocol.ts).
/// Free-form values (vars, component props, prefab overrides) are JSON text,
/// so the editor can show and send them back verbatim.
#include "../common/json_util.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace ergo::studio::plugins::scene_editor {

struct ActorSummary {
    std::string           id;
    std::string           name;
    std::string           type;
    std::string           parent;        ///< "" for a root actor
    std::array<double, 3> pos{0, 0, 0};
    std::array<double, 4> rot{0, 0, 0, 1};   ///< quaternion x, y, z, w
    std::array<double, 3> scale{1, 1, 1};
    std::string           visual_kind, visual_ref, visual_material;
    std::string           instance_of;   ///< "" when not a prefab instance
    struct Override { std::string path, value; };
    struct Var { std::string name, type, value; };
    struct Component {
        std::string                                      type;
        std::vector<std::pair<std::string, std::string>> props;
    };
    std::vector<Override>  overrides;
    std::vector<Var>       vars;
    std::vector<Component> components;
};

struct SceneInfo {
    std::string               id;
    std::string               domain;
    std::string               mount;
    std::vector<ActorSummary> actors;
    std::string               camera_mode;
    std::array<double, 3>     camera_target{0, 0, 0};
    double                    camera_distance = 8;
    double                    camera_fov_deg  = 50;
    std::string               app;
};

/// Wire form (field names as in protocol.ts: instanceOf, fov_deg, ...).
JsonValue to_json_value(const SceneInfo& s);

} // namespace ergo::studio::plugins::scene_editor
