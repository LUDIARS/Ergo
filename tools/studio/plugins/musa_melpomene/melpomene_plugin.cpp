#include "melpomene_plugin.h"

#include <cmath>
#include <cstdlib>
#include <type_traits>

namespace ergo::studio::plugins::musa_melpomene {
namespace {

/// One name for reply(): a vector becomes an array, anything else its record JSON.
template <class T>
JsonValue to_json_value_any(const std::vector<T>& v) {
    return to_json_array(v);
}
template <class T>
JsonValue to_json_value_any(const T& v) {
    return to_json_value(v);
}

/// Open PRs whose reviews are fetched per poll (one request each).
constexpr std::size_t kMaxReviewedPrs = 30;

net::HttpResponse fail(int status, const std::string& err) {
    return json_response(status, jobj({{"ok", jbool(false)}, {"err", jstr(err)}}));
}

template <class T>
net::HttpResponse reply(const ApiResult<T>& r) {
    if (!r.ok()) {
        const int st = r.status >= 400 ? r.status : (r.error.rfind("Not configured", 0) == 0 ? 503 : 502);
        return fail(st, r.error);
    }
    if constexpr (std::is_same_v<T, std::string>) return json_response(200, jstr(*r.value));
    else if constexpr (std::is_same_v<T, JsonValue>) return json_response(200, *r.value);
    else return json_response(200, to_json_value_any(*r.value));
}

std::string q(const net::HttpRequest& req, const char* key) {
    const auto it = req.query.find(key);
    return it == req.query.end() ? std::string() : it->second;
}

std::optional<double> number_of(const std::string& s) {
    if (s.empty()) return std::nullopt;
    char* e = nullptr;
    const double v = std::strtod(s.c_str(), &e);
    if (e != s.c_str() + s.size() || !std::isfinite(v)) return std::nullopt;
    return v;
}

std::optional<uint64_t> u64_of(const JsonValue* v) {
    if (!v || !v->is_number() || v->n < 0 || std::floor(v->n) != v->n) return std::nullopt;
    return static_cast<uint64_t>(v->n);
}

std::optional<uint64_t> u64_of(const std::string& s) {
    const auto d = number_of(s);
    if (!d || *d < 0 || std::floor(*d) != *d) return std::nullopt;
    return static_cast<uint64_t>(*d);
}

std::vector<std::string> strings_of(const JsonValue* v) {
    std::vector<std::string> out;
    if (v && v->is_array()) {
        for (const auto& x : *v->a) {
            if (x.is_string()) out.push_back(x.s);
        }
    }
    return out;
}

bool parse_body(const net::HttpRequest& req, JsonValue& out) {
    return ergo::common::jsonm::parse(req.body, out) && out.is_object();
}

} // namespace

MelpomenePlugin::MelpomenePlugin(ConfigStore store, std::shared_ptr<platform::IHttpClient> http, host::LogFn log)
    : info_{"musa_melpomene", "Melpomene (MUSA)",
            "Ars-Musa Melpomene: GitHub issues, comments, PRs, reviews, workflow runs and milestones, by scene and position.",
            "\xF0\x9F\x8E\xAD"},
      store_(std::move(store)),
      http_(std::move(http)),
      log_(std::move(log)),
      cache_(10) {
    auto loaded   = store_.load();
    cfg_          = std::move(loaded.config);
    config_error_ = std::move(loaded.error);
    cache_.set_duration(cfg_.cache_duration_minutes);
    if (!config_error_.empty() && log_) log_("warn", "[musa_melpomene] " + config_error_);
}

MelpomeneConfig MelpomenePlugin::config() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return cfg_;
}

GitHubApi MelpomenePlugin::api() const { return GitHubApi(http_, config()); }

bool MelpomenePlugin::update_config(const JsonValue& fields, std::string& err) {
    const auto merged = merge_config(config(), fields, err);
    if (!merged || !store_.save(*merged, err)) return false;
    std::lock_guard<std::mutex> lk(mtx_);
    const bool repo_changed = merged->repository_owner != cfg_.repository_owner || merged->repository_name != cfg_.repository_name;
    cfg_ = *merged;
    config_error_.clear();
    cache_.set_duration(cfg_.cache_duration_minutes);
    if (repo_changed) cache_.clear();
    return true;
}

ApiResult<std::vector<Ticket>> MelpomenePlugin::issues(bool force_refresh) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!force_refresh && !cache_.is_expired()) return {cache_.all(), 200, {}};
    }
    auto r = api().fetch_issues("all", {}, 1, 100);
    if (r.ok()) {
        std::lock_guard<std::mutex> lk(mtx_);
        cache_.update(*r.value);
    }
    return r;
}

ApiResult<NotificationState> MelpomenePlugin::poll_notifications() {
    const GitHubApi gh = api();
    auto prs = gh.fetch_pull_requests("open");
    if (!prs.ok()) return {std::nullopt, prs.status, prs.error};
    // The original never fetched reviews, so review_count stayed 0 and a new
    // review was never detected. Fetch them for the first open PRs.
    for (std::size_t i = 0; i < prs.value->size() && i < kMaxReviewedPrs; ++i) {
        auto& pr = (*prs.value)[i];
        auto reviews = gh.fetch_reviews(pr.number);
        if (!reviews.ok()) return {std::nullopt, reviews.status, reviews.error};
        pr.review_count = reviews.value->size();
        pr.reviews      = std::move(*reviews.value);
    }
    auto runs = gh.fetch_workflow_runs();
    if (!runs.ok()) return {std::nullopt, runs.status, runs.error};
    std::lock_guard<std::mutex> lk(mtx_);
    return {tracker_.observe(std::move(*prs.value), std::move(*runs.value)), 200, {}};
}

net::HttpResponse MelpomenePlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    const bool get  = req.method == "GET";
    const bool post = req.method == "POST";
    JsonValue  body;

    if (sub == "/api/health" && get) return json_response(200, snapshot());
    if (sub == "/api/config" && get) {
        std::lock_guard<std::mutex> lk(mtx_);
        JsonValue c = to_json_value(cfg_);
        c.set("is_configured", jbool(cfg_.is_configured()));
        c.set("error", jstr(config_error_));
        return json_response(200, c);
    }
    if (sub == "/api/config" && post) {
        if (!parse_body(req, body)) return fail(400, "invalid JSON body");
        const JsonValue* c = field(body, "config");
        std::string err;
        if (!update_config(c ? *c : body, err)) return fail(400, err);
        return json_response(200, jobj({{"ok", jbool(true)}, {"config", to_json_value(config())}}));
    }
    if (sub == "/api/issues" && get) return reply(issues(q(req, "refresh") == "1" || q(req, "refresh") == "true"));
    if (sub == "/api/issue" && get) {
        const auto n = u64_of(q(req, "number"));
        if (!n) return fail(400, "number is required");
        return reply(api().fetch_issue(*n));
    }
    if (sub == "/api/issues" && post) {
        if (!parse_body(req, body)) return fail(400, "invalid JSON body");
        const JsonValue* title = field(body, "title");
        if (!title || !title->is_string() || title->s.empty()) return fail(400, "title is required");
        // A full ticket (scene / position / priority) is rendered into the body as the Tauri app did.
        std::string text = field(body, "body") ? field(body, "body")->as_string() : std::string();
        if (const JsonValue* t = field(body, "ticket"); t && t->is_object()) {
            Ticket tk;
            tk.description = text;
            if (const JsonValue* s = field(*t, "scene_name"); s && s->is_string()) tk.scene_name = s->s;
            if (const JsonValue* p = field(*t, "object_path"); p && p->is_string()) tk.object_path = p->s;
            if (const JsonValue* w = field(*t, "world_position"); w && w->is_array() && w->a->size() == 3) {
                tk.world_position = std::array<double, 3>{(*w->a)[0].as_number(), (*w->a)[1].as_number(), (*w->a)[2].as_number()};
            }
            if (const JsonValue* p = field(*t, "priority")) tk.priority = parse_priority(p->as_string());
            if (const JsonValue* c = field(*t, "category")) tk.category = parse_category(c->as_string());
            text = tk.to_github_body();
        }
        auto labels = strings_of(field(body, "labels"));
        if (!field(body, "labels")) labels = config().default_labels;
        auto r = api().create_issue(title->s, text, labels, u64_of(field(body, "milestone")));
        if (r.ok()) {
            std::lock_guard<std::mutex> lk(mtx_);
            cache_.clear();   // the next list fetch includes the new issue
        }
        return reply(r);
    }
    if ((sub == "/api/issue/update" || sub == "/api/issue/close") && post) {
        if (!parse_body(req, body)) return fail(400, "invalid JSON body");
        const auto n = u64_of(field(body, "number"));
        if (!n) return fail(400, "number is required");
        auto r = sub == "/api/issue/close" ? api().close_issue(*n) : api().update_issue(*n, body);
        if (r.ok()) {
            std::lock_guard<std::mutex> lk(mtx_);
            cache_.clear();
        }
        return reply(r);
    }
    if (sub == "/api/comments" && get) {
        const auto n = u64_of(q(req, "issue"));
        if (!n) return fail(400, "issue is required");
        return reply(api().fetch_comments(*n));
    }
    if (sub == "/api/comments" && post) {
        if (!parse_body(req, body)) return fail(400, "invalid JSON body");
        const auto       n    = u64_of(field(body, "issue"));
        const JsonValue* text = field(body, "body");
        if (!n || !text || !text->is_string()) return fail(400, "issue and body are required");
        return reply(api().post_comment(*n, text->s));
    }
    if (sub == "/api/issues/by-scene" && get) {
        std::lock_guard<std::mutex> lk(mtx_);
        return json_response(200, to_json_array(cache_.by_scene(q(req, "scene"))));
    }
    if (sub == "/api/issues/by-object" && get) {
        std::lock_guard<std::mutex> lk(mtx_);
        return json_response(200, to_json_array(cache_.by_object_path(q(req, "path"))));
    }
    if (sub == "/api/issues/near" && get) {
        const auto x = number_of(q(req, "x")), y = number_of(q(req, "y")), z = number_of(q(req, "z"));
        const auto r = number_of(q(req, "radius"));
        if (!x || !y || !z || !r || *r < 0) return fail(400, "x, y, z and a non-negative radius are required");
        std::lock_guard<std::mutex> lk(mtx_);
        return json_response(200, to_json_array(cache_.near({*x, *y, *z}, *r)));
    }
    if (sub == "/api/cache-stats" && get) {
        std::lock_guard<std::mutex> lk(mtx_);
        return json_response(200, to_json_value(cache_.stats()));
    }
    if (sub == "/api/notifications" && get) return reply(poll_notifications());
    if (sub == "/api/pulls" && get) return reply(api().fetch_pull_requests(q(req, "state")));
    if (sub == "/api/reviews" && get) {
        const auto n = u64_of(q(req, "pr"));
        if (!n) return fail(400, "pr is required");
        return reply(api().fetch_reviews(*n));
    }
    if (sub == "/api/runs" && get) return reply(api().fetch_workflow_runs());
    if (sub == "/api/milestones" && get) return reply(api().fetch_milestones());
    if (sub == "/api/milestones" && post) {
        if (!parse_body(req, body)) return fail(400, "invalid JSON body");
        const JsonValue* title = field(body, "title");
        if (!title || !title->is_string() || title->s.empty()) return fail(400, "title is required");
        auto opt_str = [&](const char* k) -> std::optional<std::string> {
            const JsonValue* v = field(body, k);
            return v && v->is_string() ? std::optional<std::string>(v->s) : std::nullopt;
        };
        return reply(api().create_milestone(title->s, opt_str("description"), opt_str("due_on")));
    }
    if (sub == "/api/user" && get) return reply(api().fetch_authenticated_user());
    return fail(404, "not found");
}

JsonValue MelpomenePlugin::snapshot() const {
    std::lock_guard<std::mutex> lk(mtx_);
    // Not configured yet is normal; a broken settings file or store is not.
    JsonValue h = jobj({{"ok", jbool(config_error_.empty())}, {"configured", jbool(cfg_.is_configured())},
                        {"has_token", jbool(cfg_.github_token && !cfg_.github_token->empty())},
                        {"cached", jnum(static_cast<double>(cache_.all().size()))}});
    if (!config_error_.empty()) h.set("err", jstr(config_error_));
    return h;
}

std::unique_ptr<host::IPlugin> make_melpomene_plugin(const std::string& home, const host::LogFn& log) {
    return std::make_unique<MelpomenePlugin>(ConfigStore(settings_path(home), platform::make_system_credential_store()),
                                             platform::make_system_http_client(), log);
}

} // namespace ergo::studio::plugins::musa_melpomene
