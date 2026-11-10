#include "vfx_breed_plugin.h"

#include "../particle/particle_schema.h"
#include "../vfx_suggest/vfx_catalog.h"
#include "breed_genes.h"

namespace ergo::studio::plugins::vfx_breed {
namespace {

std::string lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

} // namespace

JsonValue to_json_value(const GenerationResult& r) {
    JsonValue children = JsonValue::make_array();
    for (const auto& c : r.children) {
        JsonValue from = JsonValue::make_array();
        for (int i : c.parents) from.push(jnum(i));
        children.push(jobj({{"config", c.config}, {"parents", from}}));
    }
    JsonValue dir = jobj({{"status", jstr(r.direction_status)}});
    if (r.direction_status == "applied") dir.set("weights", to_json_value(r.weights));
    if (!r.reason.empty()) dir.set("reason", jstr(r.reason));
    return jobj({{"ok", jbool(true)}, {"children", children}, {"seed", jnum(r.seed)},
                 {"generation", jnum(r.generation)}, {"direction", dir}});
}

VfxBreedPlugin::VfxBreedPlugin(VfxBreedSettings s, std::shared_ptr<platform::IProcessRunner> runner, host::LogFn log)
    : info_{"vfx-breed", "VFX Breed", "Evolve particle effects by picking favourites from a generation of variants.",
            "\xF0\x9F\xA7\xAC"},
      settings_(std::move(s)),
      cli_(std::move(runner), settings_.bin, settings_.model),
      log_(std::move(log)) {}

std::string VfxBreedPlugin::advisor_reason() const {
    const std::string b = lower(settings_.backend);
    if (b == "sdk") return "ERGO_VFX_BACKEND=sdk is not available in the native Studio (only \"cli\")";
    if (b != "cli") return "unknown ERGO_VFX_BACKEND=\"" + settings_.backend + "\"";
    return cli_.not_ready_reason();
}

JsonValue VfxBreedPlugin::advisor_health() const {
    const std::string why = advisor_reason();
    JsonValue h = jobj({{"ok", jbool(why.empty())}, {"backend", jstr(lower(settings_.backend))},
                        {"model", jstr(why.empty() ? settings_.model : std::string())}});
    if (!why.empty()) h.set("reason", jstr(why));
    return h;
}

JsonValue VfxBreedPlugin::snapshot() const { return jobj({{"ok", jbool(true)}, {"advisor", advisor_health()}}); }

GenerationResult VfxBreedPlugin::run_generation(const GenerationRequest& req) const {
    GenerationResult r;
    std::optional<std::vector<double>> bias;
    if (!req.direction.empty()) {
        if (const std::string why = advisor_reason(); !why.empty()) {
            r.direction_status = "unavailable";
            r.reason           = why;
        } else {
            const auto reply = cli_.ask(build_direction_prompt(req.direction, to_config(req.parents[0])));
            const auto w     = reply.ok ? parse_weights(reply.text) : std::nullopt;
            if (!reply.ok) {
                r.direction_status = "failed";
                r.reason           = reply.error;
            } else if (!w) {
                r.direction_status = "failed";
                r.reason           = "parse failed: no JSON in the advisor reply";
            } else {
                bias               = to_bias(*w);
                r.weights          = *w;
                r.direction_status = "applied";
            }
        }
    }
    r.generation = req.generation + 1;
    r.seed       = req.seed;
    BreedInput in;
    in.parents    = req.parents;
    in.count      = req.count;
    in.strength   = req.strength;
    in.seed       = req.seed;
    in.bias       = bias;
    in.generation = r.generation;
    r.children    = breed(in);
    return r;
}

net::HttpResponse VfxBreedPlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    const bool get = req.method == "GET";
    if (sub == "/api/health" && get) return json_response(200, snapshot());
    if (sub == "/api/seeds" && get) {
        JsonValue a = JsonValue::make_array();
        a.push(jobj({{"id", jstr("default")}, {"name", jstr("\xE6\x97\xA2\xE5\xAE\x9A\xE3\x82\xA8\xE3\x83\x95\xE3\x82\xA7\xE3\x82\xAF\xE3\x83\x88")},
                     {"source", jstr("default")}, {"config", particle::default_effect()}}));
        const auto& cat = vfx_suggest::catalog();
        for (std::size_t i = 0; i < cat.size(); ++i) {
            a.push(jobj({{"id", jstr("catalog-" + std::to_string(i))}, {"name", jstr(cat[i].name)},
                         {"source", jstr("catalog")}, {"config", cat[i].config}}));
        }
        return json_response(200, a);
    }
    if (sub == "/api/genes" && get) {
        JsonValue a = JsonValue::make_array();
        for (const auto& g : genes()) {
            a.push(jobj({{"id", jstr(g.id)}, {"label", jstr(g.label)},
                         {"range", jobj({{"lo", jnum(g.lo)}, {"hi", jnum(g.hi)}})},
                         {"scale", jstr(g.scale == GeneScale::Log ? "log" : "linear")}, {"wrap", jbool(g.wrap)}}));
        }
        return json_response(200, a);
    }
    if (sub == "/api/generation" && req.method == "POST") {
        JsonValue body;
        if (!ergo::common::jsonm::parse(req.body, body)) body = JsonValue::make_null();
        const auto parsed = parse_generation_request(body, fresh_seed);
        if (const auto* err = std::get_if<std::string>(&parsed)) {
            return json_response(400, jobj({{"ok", jbool(false)}, {"err", jstr(*err)}}));
        }
        const auto& gr = std::get<GenerationRequest>(parsed);
        const auto r = run_generation(gr);
        if (log_) {
            if (r.direction_status == "failed") log_("warn", "vfx-breed: direction ignored: " + r.reason);
            log_("info", "vfx-breed: gen " + std::to_string(r.generation) + " from " + std::to_string(gr.parents.size()) +
                             " parent(s) \xE2\x86\x92 " + std::to_string(r.children.size()) + " children (seed " +
                             std::to_string(static_cast<uint32_t>(r.seed)) + ", direction " + r.direction_status + ")");
        }
        return json_response(200, to_json_value(r));
    }
    return json_response(404, jobj({{"ok", jbool(false)}, {"err", jstr("not found")}}));
}

std::unique_ptr<host::IPlugin> make_vfx_breed_plugin(VfxBreedSettings s, const host::LogFn& log) {
    return std::make_unique<VfxBreedPlugin>(std::move(s), platform::make_system_process_runner(), log);
}

} // namespace ergo::studio::plugins::vfx_breed
