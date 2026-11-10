#include "vfx_suggest_plugin.h"

#include "../particle/particle_schema.h"
#include "ks_export.h"
#include "vfx_prompt.h"
#include "vfx_sanitize.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ergo::studio::plugins::vfx_suggest {
namespace {

net::HttpResponse fail(int status, const std::string& err) {
    return json_response(status, jobj({{"ok", jbool(false)}, {"err", jstr(err)}}));
}

std::string lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

/// count as JS did: Number(String(count)), finite or 3, then 1..4.
int count_of(const JsonValue* v) {
    double n = 3;
    if (v && v->is_number() && std::isfinite(v->n)) n = v->n;
    else if (v && v->is_string()) {
        char* e = nullptr;
        const double d = std::strtod(v->s.c_str(), &e);
        if (!v->s.empty() && e == v->s.c_str() + v->s.size() && std::isfinite(d)) n = d;
    }
    return static_cast<int>(std::min(4.0, std::max(1.0, std::floor(n))));
}

} // namespace

VfxSuggestPlugin::VfxSuggestPlugin(VfxSuggestSettings s, std::shared_ptr<platform::IProcessRunner> runner, host::LogFn log)
    : info_{"vfx-suggest", "VFX Suggest", "Generate ParticleEffectConfig presets from game + scene context via LLM.",
            "\xF0\x9F\x8E\x86"},
      settings_(std::move(s)),
      cli_(std::move(runner), settings_.bin, settings_.model),
      log_(std::move(log)) {}

std::string VfxSuggestPlugin::not_ready_reason() const {
    const std::string b = lower(settings_.backend);
    if (b == "sdk") return "ERGO_VFX_BACKEND=sdk is not available in the native Studio (only \"cli\")";
    if (b != "cli") return "Unknown ERGO_VFX_BACKEND=\"" + settings_.backend + "\" \xE2\x80\x94 expected \"cli\"";
    return cli_.not_ready_reason();
}

JsonValue VfxSuggestPlugin::snapshot() const {
    const std::string why = not_ready_reason();
    JsonValue h = jobj({{"ok", jbool(why.empty())}, {"backend", jstr(lower(settings_.backend))}, {"model", jstr(settings_.model)}});
    if (!why.empty()) h.set("reason", jstr(why));
    return h;
}

VfxSuggestPlugin::Generated VfxSuggestPlugin::generate(const std::string& game, const std::string& scene, int count) const {
    Generated g;
    if (const std::string why = not_ready_reason(); !why.empty()) {
        g.status = 503;
        g.error  = "backend not ready: " + why;
        return g;
    }
    const int n = std::min(4, std::max(1, count));
    const std::string prompt = build_system_prompt(game) + "\n\n---\n\n" + build_user_content(game, scene, n);
    const auto reply = cli_.ask(prompt);
    if (!reply.ok) {
        g.status = 502;
        g.error  = reply.error;
        return g;
    }
    JsonValue data;
    if (!ergo::common::jsonm::parse(extract_json(reply.text), data)) {
        g.status = 502;
        g.error  = "parse failed\nraw: " + reply.text.substr(0, 200);
        return g;
    }
    const JsonValue* presets = field(data, "presets");
    g.ok      = true;
    g.presets = sanitize_presets(presets ? *presets : JsonValue::make_array());
    return g;
}

net::HttpResponse VfxSuggestPlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    const bool get  = req.method == "GET";
    const bool post = req.method == "POST";
    if (sub == "/api/health" && get) return json_response(200, snapshot());
    if (sub == "/api/games" && get) {
        JsonValue a = JsonValue::make_array();
        for (const auto& [id, label] : game_labels()) a.push(jobj({{"id", jstr(id)}, {"label", jstr(label)}}));
        return json_response(200, a);
    }
    if (sub == "/api/catalog" && get) {
        // As in TS: with ?game= the first 8 entries (filtering happens client side).
        const bool by_game = req.query.count("game") > 0;
        JsonValue a = JsonValue::make_array();
        for (std::size_t i = 0; i < catalog().size(); ++i) {
            if (by_game && i >= 8) break;
            a.push(to_json_value(catalog()[i]));
        }
        return json_response(200, a);
    }
    if (sub == "/api/generate" && post) {
        JsonValue body;
        if (!ergo::common::jsonm::parse(req.body, body) || !body.is_object()) return fail(400, "request body must be JSON");
        const JsonValue* game  = field(body, "game");
        const JsonValue* scene = field(body, "scene");
        if (!game || !game->is_string() || !is_game_id(game->s)) {
            return fail(400, "game must be one of: ks, ac, ul, generic");
        }
        if (!scene || !scene->is_string() || trim(scene->s).empty()) return fail(400, "scene must be a non-empty string");
        const auto g = generate(game->s, trim(scene->s), count_of(field(body, "count")));
        if (!g.ok) {
            if (log_ && g.status == 502) log_("error", "vfx-suggest generate failed: " + g.error);
            return fail(g.status, g.error);
        }
        if (log_) log_("info", "vfx-suggest: " + game->s + " \"" + trim(scene->s) + "\" \xE2\x86\x92 " +
                                   std::to_string(g.presets.size()) + " presets");
        JsonValue arr = JsonValue::make_array();
        for (const auto& p : g.presets) arr.push(to_json_value(p));
        return json_response(200, jobj({{"ok", jbool(true)}, {"presets", arr}}));
    }
    if (sub == "/api/ks-emitter" && post) {
        JsonValue body;
        if (!ergo::common::jsonm::parse(req.body, body) || !body.is_object()) {
            return fail(400, "request body must be ParticleEffectConfig JSON");
        }
        const auto key = req.query.find("key");
        const JsonValue cfg = particle::merge_config(particle::default_effect(), body);
        const JsonValue emitter = to_ks_emitter_json(cfg, key == req.query.end() ? std::string() : key->second);
        if (log_) log_("info", "vfx-suggest: ks-emitter export \"" + field(emitter, "name")->as_string() + "\"");
        return json_response(200, jobj({{"ok", jbool(true)}, {"emitter", emitter}}));
    }
    return fail(404, "not found");
}

std::unique_ptr<host::IPlugin> make_vfx_suggest_plugin(VfxSuggestSettings s, const host::LogFn& log) {
    return std::make_unique<VfxSuggestPlugin>(std::move(s), platform::make_system_process_runner(), log);
}

} // namespace ergo::studio::plugins::vfx_suggest
