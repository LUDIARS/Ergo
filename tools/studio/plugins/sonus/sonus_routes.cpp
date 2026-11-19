#include "sonus_routes.h"

#include "sonus_shown_json.h"

#include "../common/json_util.h"
#include "../common/mutation_guard.h"

#include <cmath>

namespace ergo::studio::plugins::sonus {
namespace {

net::HttpResponse error_response(int status, const char* code, const std::string& message) {
    return json_response(status, jobj({{"error", jstr(code)}, {"message", jstr(message)}}));
}

net::HttpResponse edit_response(Edit e, const char* what) {
    if (e == Edit::NotFound) return error_response(404, "not_found", std::string("no such ") + what);
    return json_response(200, jobj({{"ok", jbool(true)}, {"changed", jbool(e == Edit::Changed)}}));
}

const std::string* string_field(const JsonValue& body, const char* key) {
    const JsonValue* v = field(body, key);
    return v && v->is_string() ? &v->s : nullptr;
}

net::HttpResponse bus(SonusPlugin& plugin, const JsonValue& body) {
    const std::string* key = string_field(body, "bus");
    if (!key) return error_response(400, "invalid_request", "bus (the bus key, a string) is missing");
    const JsonValue* fader = field(body, "fader_db");
    const JsonValue* mute  = field(body, "mute");
    const JsonValue* solo  = field(body, "solo");
    if ((fader != nullptr) + (mute != nullptr) + (solo != nullptr) != 1) {
        return error_response(400, "invalid_request", "give exactly one of fader_db (number), mute (bool), solo (bool)");
    }
    if (fader) {
        if (!fader->is_number() || !std::isfinite(fader->n)) {
            return error_response(400, "invalid_request", "fader_db must be a finite number (dB; -144 = off)");
        }
        return edit_response(plugin.set_bus_fader(*key, static_cast<float>(fader->n)), "bus");
    }
    const JsonValue* flag = mute ? mute : solo;
    if (!flag->is_bool()) return error_response(400, "invalid_request", std::string(mute ? "mute" : "solo") + " must be a bool");
    return edit_response(mute ? plugin.set_bus_mute(*key, flag->b) : plugin.set_bus_solo(*key, flag->b), "bus");
}

net::HttpResponse recording_save(SonusPlugin& plugin) {
    std::string       error;
    const std::string path = plugin.save_recording(error);
    if (path.empty()) {
        const bool off = plugin.config().recording_dir.empty();
        return error_response(off ? 503 : 409, off ? "saving_off" : "not_saved", error);
    }
    return json_response(200, jobj({{"ok", jbool(true)}, {"path", jstr(path)}}));
}

/// Mutating routes; `body` is the parsed JSON object.
net::HttpResponse mutate(SonusPlugin& plugin, const std::string& sub, const JsonValue& body) {
    if (sub == "/api/connect") {
        const std::string why = plugin.connect();
        if (!why.empty()) {
            const bool open = plugin.shown().state != LinkState::Offline;
            return error_response(open ? 409 : 502, open ? "already_connected" : "connect_failed", why);
        }
        return json_response(200, jobj({{"ok", jbool(true)}}));
    }
    if (sub == "/api/disconnect") {
        plugin.disconnect();
        return json_response(200, jobj({{"ok", jbool(true)}}));
    }
    if (sub == "/api/mixer/bus") return bus(plugin, body);
    if (sub == "/api/mixer/undo") return edit_response(plugin.undo(), "edit");
    if (sub == "/api/mixer/redo") return edit_response(plugin.redo(), "edit");
    if (sub == "/api/overrides/revert") {
        const std::string* key = string_field(body, "key");
        if (!key) return error_response(400, "invalid_request", "key (the override key, a string) is missing");
        return edit_response(plugin.revert(*key), "override");
    }
    if (sub == "/api/overrides/revert-all") return edit_response(plugin.revert_all(), "override");
    if (sub == "/api/recording/start" || sub == "/api/recording/stop") {
        const bool start = sub == "/api/recording/start";
        if (!(start ? plugin.start_recording() : plugin.stop_recording())) {
            return error_response(409, start ? "cannot_start" : "cannot_stop",
                                  start ? "no connected game, or a recording is already running"
                                        : "no recording is running");
        }
        return json_response(200, jobj({{"ok", jbool(true)}}));
    }
    return recording_save(plugin);   // "/api/recording/save" (the only route left, see is_route)
}

bool is_mutating_route(const std::string& sub) {
    for (const char* r : {"/api/connect", "/api/disconnect", "/api/mixer/bus", "/api/mixer/undo", "/api/mixer/redo",
                          "/api/overrides/revert", "/api/overrides/revert-all", "/api/recording/start",
                          "/api/recording/stop", "/api/recording/save"}) {
        if (sub == r) return true;
    }
    return false;
}

} // namespace

net::HttpResponse handle_sonus_route(SonusPlugin& plugin, const net::HttpRequest& req, const std::string& sub,
                                     const host::LogFn& log) {
    const bool state = sub == "/api/state";
    if (!state && !is_mutating_route(sub)) return error_response(404, "not_found", "not found");
    if (req.method != (state ? "GET" : "POST")) return error_response(405, "method_not_allowed", "method not allowed");
    if (const auto why = check_mutation(req.method, req.header("host"), req.header("origin"), req.header("content-type"))) {
        if (log) log("warn", "[sonus] rejected " + req.method + " " + sub + ": " + *why);
        return error_response(403, "forbidden", *why);
    }
    if (state) return json_response(200, sonus_shown_json(plugin.shown()));
    JsonValue body;
    if (!ergo::common::jsonm::parse(req.body, body) || !body.is_object()) {
        return error_response(400, "invalid_request", "the body must be a JSON object");
    }
    return mutate(plugin, sub, body);
}

} // namespace ergo::studio::plugins::sonus
