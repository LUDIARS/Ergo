#include "view_registry.h"

#include "explorer_view.h"
#include "log_view.h"
#include "lookdev_view.h"
#include "musa_views.h"
#include "studio_view.h"
#include "thaleia_view.h"
#include "vfx_suggest_view.h"

namespace ergo::studio::views {

std::unique_ptr<host::IPluginView> make_view(host::IPlugin& plugin, const PluginFinder& find) {
    const std::string& id = plugin.info().id;
    if (id == "studio") return std::make_unique<StudioView>(plugin);
    if (id == "log") return std::make_unique<LogView>(plugin);
    if (id == "lookdev") return std::make_unique<LookdevView>(plugin, find ? find("variable") : nullptr);
    if (id == "musa_thaleia") return std::make_unique<ThaleiaView>(plugin);
    if (id == "musa_terpsichore") return std::make_unique<TerpsichoreView>(plugin);
    if (id == "musa_melpomene") return std::make_unique<MelpomeneView>(plugin);
    if (id == "vfx-suggest") return std::make_unique<VfxSuggestView>(plugin, find ? find("particle") : nullptr);
    return std::make_unique<ExplorerView>(plugin);
}

ui::ViewLookup ViewRegistry::lookup() {
    return [this](host::IPlugin& p) -> host::IPluginView* {
        auto it = views_.find(&p);
        if (it == views_.end()) {
            PluginFinder find = [this](const std::string& id) -> host::IPlugin* {
                if (!host_) return nullptr;
                for (host::IPlugin* q : host_->plugins()) {
                    if (q->info().id == id) return q;
                }
                return nullptr;
            };
            it = views_.emplace(&p, make_view(p, find)).first;
        }
        return it->second.get();
    };
}

} // namespace ergo::studio::views
