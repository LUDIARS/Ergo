#include "builtin_plugins.h"

#include "plugins/log/log_plugin.h"
#include "plugins/musa_melpomene/melpomene_plugin.h"
#include "plugins/musa_terpsichore/terpsichore_plugin.h"
#include "plugins/musa_thaleia/thaleia_plugin.h"
#include "plugins/particle/particle_plugin.h"
#include "plugins/pictor_perf/pictor_perf_plugin.h"
#include "plugins/profile/profile_plugin.h"
#include "plugins/render_pipeline/render_pipeline_plugin.h"
#include "plugins/rive/rive_plugin.h"
#include "plugins/scene_editor/scene_editor_plugin.h"
#include "plugins/studio/studio_plugin.h"
#include "plugins/ui_layout/ui_layout_plugin.h"
#include "plugins/variable/variable_plugin.h"
#include "plugins/vfx_breed/vfx_breed_plugin.h"
#include "plugins/vfx_suggest/vfx_suggest_plugin.h"
#include "plugins/visus/visus_plugin.h"

namespace ergo::studio::app {

std::vector<host::PluginFactory> builtin_plugins(const StudioSettings& s) {
    namespace p = ergo::studio::plugins;
    std::vector<host::PluginFactory> f;
    f.push_back([s](const host::LogFn& log) {
        return p::studio::make_studio_plugin(s.studio, std::filesystem::u8path(s.sdk_dir), log);
    });
    f.push_back(&p::variable::make_variable_plugin);
    f.push_back(&p::particle::make_particle_plugin);
    f.push_back(&p::scene_editor::make_scene_editor_plugin);
    f.push_back(&p::log::make_log_plugin);
    f.push_back(&p::profile::make_profile_plugin);
    f.push_back(&p::pictor_perf::make_pictor_perf_plugin);
    f.push_back([dir = s.pictor_profile_dir](const host::LogFn& log) {
        return p::render_pipeline::make_render_pipeline_plugin(dir, log);
    });
    f.push_back(&p::rive::make_rive_plugin);
    f.push_back([roots = s.visus_roots](const host::LogFn& log) { return p::visus::make_visus_plugin(roots, log); });
    f.push_back([root = s.ui_layout_root](const host::LogFn& log) {
        return p::ui_layout::make_ui_layout_plugin(root.empty() ? std::filesystem::path() : std::filesystem::u8path(root), log);
    });
    f.push_back([s](const host::LogFn& log) {
        return p::vfx_suggest::make_vfx_suggest_plugin({s.vfx_backend, s.vfx_model, s.claude_bin}, log);
    });
    f.push_back([s](const host::LogFn& log) {
        return p::vfx_breed::make_vfx_breed_plugin({s.vfx_backend, s.vfx_model, s.claude_bin}, log);
    });
    f.push_back([home = s.user_home](const host::LogFn& log) { return p::musa_melpomene::make_melpomene_plugin(home, log); });
    f.push_back([home = s.user_home](const host::LogFn& log) {
        return p::musa_terpsichore::make_terpsichore_plugin(home, log);
    });
    f.push_back([home = s.user_home](const host::LogFn& log) {
        return p::musa_thaleia::make_thaleia_plugin(p::musa_thaleia::default_thaleia_root(home), log);
    });
    return f;
}

} // namespace ergo::studio::app
