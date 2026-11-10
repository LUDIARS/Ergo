#include "ergo/studio/host/health_report.h"

namespace ergo::studio::host {

using ergo::common::jsonm::JsonValue;

namespace {
JsonValue failed(const std::string& err) {
    JsonValue h = JsonValue::make_object();
    h.set("ok", JsonValue::make_bool(false));
    h.set("err", JsonValue::make_string(err));
    return h;
}
} // namespace

JsonValue plugin_health(IPlugin& p) {
    JsonValue h;
    try {
        IPluginHealth* src = p.health();
        if (!src) {
            h = JsonValue::make_object();
            h.set("ok", JsonValue::make_bool(true));
        } else {
            h = src->snapshot();
            const JsonValue* ok = h.is_object() ? h.find("ok") : nullptr;
            if (!ok || !ok->is_bool()) h = failed("health() must return { ok: boolean }");
        }
    } catch (const std::exception& e) {
        h = failed(std::string("health() threw: ") + e.what());
    } catch (...) {
        h = failed("health() threw");
    }
    // Copy into a fresh object so a plugin that hands out a shared JsonValue is not mutated.
    JsonValue out = JsonValue::make_object();
    for (const auto& [k, v] : *h.o) out.set(k, v);
    out.set("id", JsonValue::make_string(p.info().id));
    out.set("title", JsonValue::make_string(p.info().title));
    return out;
}

std::string health_report(const std::vector<IPlugin*>& plugins, const std::vector<RejectedPlugin>& rejected) {
    bool all_ok = rejected.empty();
    JsonValue list = JsonValue::make_array();
    for (IPlugin* p : plugins) {
        JsonValue h = plugin_health(*p);
        if (!h.find("ok")->as_bool()) all_ok = false;
        list.push(std::move(h));
    }
    JsonValue rej = JsonValue::make_array();
    for (const auto& r : rejected) {
        JsonValue e = JsonValue::make_object();
        e.set("id", JsonValue::make_string(r.id));
        e.set("source", JsonValue::make_string(r.source));
        e.set("ok", JsonValue::make_bool(false));
        e.set("err", JsonValue::make_string(r.reason));
        rej.push(std::move(e));
    }
    JsonValue root = JsonValue::make_object();
    root.set("ok", JsonValue::make_bool(all_ok));
    root.set("plugins", std::move(list));
    root.set("rejected", std::move(rej));
    return ergo::common::jsonm::serialize(root);
}

std::string plugin_list(const std::vector<IPlugin*>& plugins) {
    JsonValue list = JsonValue::make_array();
    for (IPlugin* p : plugins) {
        const PluginInfo& i = p->info();
        JsonValue e = JsonValue::make_object();
        e.set("id", JsonValue::make_string(i.id));
        e.set("title", JsonValue::make_string(i.title));
        e.set("description", JsonValue::make_string(i.description));
        e.set("icon", JsonValue::make_string(i.icon));
        if (p->socket()) e.set("ws", JsonValue::make_string("/" + i.id + "/ws"));
        list.push(std::move(e));
    }
    JsonValue root = JsonValue::make_object();
    root.set("plugins", std::move(list));
    return ergo::common::jsonm::serialize(root);
}

} // namespace ergo::studio::host
