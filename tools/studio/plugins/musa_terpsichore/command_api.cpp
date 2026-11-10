#include "command_api.h"

namespace ergo::studio::plugins::musa_terpsichore {
namespace {

JsonValue opt_str(const std::optional<std::string>& s) { return s ? jstr(*s) : JsonValue::make_null(); }

net::HttpResponse result(const CommandResult& r) { return json_response(200, to_json_value(r)); }

net::HttpResponse ok(const std::string& msg) { return result({true, msg, JsonValue::make_null()}); }

} // namespace

JsonValue to_json_value(const CommandResult& r) {
    JsonValue o = jobj({{"success", jbool(r.success)}, {"message", jstr(r.message)}});
    if (!r.data.is_null()) o.set("data", r.data);
    return o;
}

CommandApi::CommandApi(TerpsichoreConfig cfg) : cfg_(std::move(cfg)), started_(std::chrono::steady_clock::now()) {}

void CommandApi::set_command_handler(CommandHandler h) {
    std::lock_guard<std::mutex> lk(mtx_);
    handler_ = std::move(h);
}

void CommandApi::set_game_scene(std::optional<std::string> scene, JsonValue custom_data) {
    std::lock_guard<std::mutex> lk(mtx_);
    scene_       = std::move(scene);
    custom_data_ = std::move(custom_data);
}

JsonValue CommandApi::status() const {
    std::lock_guard<std::mutex> lk(mtx_);
    const auto up = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started_).count();
    return jobj({{"running", jbool(true)}, {"port", jnum(cfg_.port)}, {"role", jstr(role_name(cfg_.role))},
                 {"uptime_seconds", jnum(static_cast<double>(up))},
                 {"request_count", jnum(static_cast<double>(request_count_))}});
}

bool CommandApi::authorized(const net::HttpRequest& req) const {
    if (!cfg_.requires_auth()) return true;
    const auto h = req.header("authorization");
    return h && *h == "Bearer " + cfg_.token;
}

net::HttpResponse CommandApi::handle(const net::HttpRequest& req) {
    const std::string& p = req.path;
    if (req.method == "GET") {
        if (p == "/api/health") return json_response(200, jobj({{"status", jstr("ok")}, {"service", jstr("terpsichore")}}));
        if (p == "/api/status") return json_response(200, status());
        std::lock_guard<std::mutex> lk(mtx_);
        if (p == "/api/compile-status") {
            return json_response(200, jobj({{"compiling", jbool(compiling_)}, {"errors", JsonValue::make_array()},
                                            {"warnings", JsonValue::make_array()}}));
        }
        if (p == "/api/build-status") {
            return json_response(200, jobj({{"building", jbool(building_)},
                                            {"success", build_success_ ? jbool(*build_success_) : JsonValue::make_null()},
                                            {"output_path", opt_str(build_output_)}, {"errors", JsonValue::make_array()}}));
        }
        if (p == "/api/game-status" || p == "/api/play-status") {
            return json_response(200, jobj({{"playing", jbool(playing_)}, {"scene", opt_str(scene_)},
                                            {"custom_data", custom_data_}}));
        }
        return net::HttpResponse::json(404, "{\"success\":false,\"message\":\"not found\"}");
    }
    if (req.method != "POST") return net::HttpResponse::json(405, "{\"success\":false,\"message\":\"method not allowed\"}");
    if (!authorized(req)) return net::HttpResponse::json(401, "{\"success\":false,\"message\":\"unauthorized\"}");

    if (p == "/api/execute-command") {
        JsonValue body;
        const JsonValue* cmd = nullptr;
        if (!ergo::common::jsonm::parse(req.body, body) || !(cmd = field(body, "command")) || !cmd->is_string()) {
            return net::HttpResponse::json(400, "{\"success\":false,\"message\":\"body must be {command, args?, payload?}\"}");
        }
        CommandRequest r{cmd->s, {}, JsonValue::make_null()};
        if (const JsonValue* args = field(body, "args")) {
            if (!args->is_array()) return net::HttpResponse::json(400, "{\"success\":false,\"message\":\"args must be an array\"}");
            for (const auto& a : *args->a) {
                if (!a.is_string()) return net::HttpResponse::json(400, "{\"success\":false,\"message\":\"args must be strings\"}");
                r.args.push_back(a.s);
            }
        }
        if (const JsonValue* payload = field(body, "payload")) r.payload = deep_copy(*payload);
        CommandHandler h;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            ++request_count_;
            h = handler_;
        }
        // The handler runs outside the lock so a slow command does not block status reads.
        if (h) return result(h(r));
        JsonValue args = JsonValue::make_array();
        for (const auto& a : r.args) args.push(jstr(a));
        return result({true, "Command received",
                       jobj({{"command", jstr(r.command)}, {"args", args}, {"note", jstr("No command handler registered")}})});
    }

    std::lock_guard<std::mutex> lk(mtx_);
    if (p == "/api/recompile") {
        ++request_count_;
        compiling_ = true;
        return ok("Recompilation triggered");
    }
    if (p == "/api/build") {
        JsonValue body;
        const JsonValue* target = nullptr;
        if (!ergo::common::jsonm::parse(req.body, body) || !(target = field(body, "target")) || !target->is_string()) {
            return net::HttpResponse::json(400, "{\"success\":false,\"message\":\"body must be {target, output_path?, development}\"}");
        }
        ++request_count_;
        building_      = true;
        build_success_ = std::nullopt;
        const JsonValue* out = field(body, "output_path");
        build_output_  = out && out->is_string() ? std::optional<std::string>(out->s) : std::nullopt;
        return ok("Build started for target: " + target->s);
    }
    if (p == "/api/play-start" || p == "/api/play-stop") {
        ++request_count_;
        playing_ = p == "/api/play-start";
        return ok(playing_ ? "Play mode started" : "Play mode stopped");
    }
    return net::HttpResponse::json(404, "{\"success\":false,\"message\":\"not found\"}");
}

} // namespace ergo::studio::plugins::musa_terpsichore
