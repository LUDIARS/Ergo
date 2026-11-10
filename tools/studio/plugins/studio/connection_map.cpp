#include "connection_map.h"

#include <cstring>
#include <tuple>

namespace ergo::studio::plugins::studio {
namespace {

struct Info {
    Service     key;
    const char* label;
    const char* role;
};

const std::vector<Info>& infos() {
    static const std::vector<Info> v{
        {Service::Excubitor, "Excubitor (Ex)", "接続先 (catalog / topology) と起動管理"},
        {Service::Concordia, "Concordia (Cc)", "登録プロジェクトと AI への実装指示"},
        {Service::Actio, "Actio (A)", "タスク管理"},
        {Service::Breviarium, "Breviarium (Br)", "プロジェクト状態の要約"},
        {Service::Curare, "Curare (Cu)", "リソースの版・提出・取得"},
        {Service::Revisor, "Revisor", "ソースの local PR・審査・マージ"},
        {Service::Cernere, "Cernere (Cr)", "認証・所属"},
    };
    return v;
}

JsonValue opt(const std::optional<std::string>& s) { return s ? jstr(*s) : JsonValue::make_null(); }

} // namespace

const char* node_state(const ProbeResult* p) {
    if (!p) return "not_configured";
    if (p->ok) return "ok";
    switch (p->failure.value_or(FailureKind::Unreachable)) {
        case FailureKind::NotConfigured: return "not_configured";
        case FailureKind::Unauthorized:
        case FailureKind::Forbidden: return "unauthorized";
        case FailureKind::Rejected:
        case FailureKind::NotFound:
        case FailureKind::BadResponse: return "degraded";
        default: return "down";
    }
}

JsonValue to_json_value(const ProbeResult& p) {
    JsonValue o = jobj({{"key", jstr(service_key(p.key))}, {"url", opt(p.url)}, {"ok", jbool(p.ok)},
                        {"checkedAt", jstr(p.checked_at)}, {"lastOkAt", opt(p.last_ok_at)}});
    if (p.failure) o.set("failure", jstr(failure_name(*p.failure)));
    if (!p.detail.empty()) o.set("detail", jstr(p.detail));
    if (!p.auth.empty()) o.set("auth", jstr(p.auth));
    return o;
}

JsonValue build_connection_map(const std::vector<ProbeResult>& probes) {
    auto find = [&](Service s) -> const ProbeResult* {
        for (const auto& p : probes) {
            if (p.key == s) return &p;
        }
        return nullptr;
    };
    JsonValue nodes = JsonValue::make_array();
    nodes.push(jobj({{"id", jstr("studio")}, {"label", jstr("Ergo Studio")}, {"role", jstr("このアプリ")}, {"state", jstr("ok")},
                     {"url", JsonValue::make_null()}, {"detail", JsonValue::make_null()}, {"auth", JsonValue::make_null()},
                     {"lastOkAt", JsonValue::make_null()}}));
    for (const auto& i : infos()) {
        const ProbeResult* p = find(i.key);
        nodes.push(jobj({{"id", jstr(service_key(i.key))}, {"label", jstr(i.label)}, {"role", jstr(i.role)},
                         {"state", jstr(node_state(p))}, {"url", p ? opt(p->url) : JsonValue::make_null()},
                         {"detail", p && !p->detail.empty() ? jstr(p->detail) : JsonValue::make_null()},
                         {"auth", p && !p->auth.empty() ? jstr(p->auth) : JsonValue::make_null()},
                         {"lastOkAt", p ? opt(p->last_ok_at) : JsonValue::make_null()}}));
    }
    auto alive = [&](Service s) { return std::strcmp(node_state(find(s)), "ok") == 0; };

    JsonValue edges = JsonValue::make_array();
    const std::pair<Service, const char*> studio_edges[] = {
        {Service::Excubitor, "接続先の解決"}, {Service::Concordia, "プロジェクト / 実装指示"}, {Service::Actio, "タスク"},
        {Service::Breviarium, "状態の要約"},  {Service::Curare, "リソース同期"},              {Service::Revisor, "PR の状態"}};
    for (const auto& [to, label] : studio_edges) {
        edges.push(jobj({{"from", jstr("studio")}, {"to", jstr(service_key(to))}, {"label", jstr(label)}, {"active", jbool(alive(to))}}));
    }
    const std::tuple<Service, Service, const char*> service_edges[] = {
        {Service::Actio, Service::Concordia, "プロジェクト同期"}, {Service::Concordia, Service::Actio, "タスク登録・状態"},
        {Service::Concordia, Service::Revisor, "local PR 提出"},  {Service::Breviarium, Service::Actio, "スプリント集計"},
        {Service::Curare, Service::Cernere, "認可・所属"},        {Service::Actio, Service::Cernere, "認証 (トークン時)"}};
    for (const auto& [from, to, label] : service_edges) {
        edges.push(jobj({{"from", jstr(service_key(from))}, {"to", jstr(service_key(to))}, {"label", jstr(label)},
                         {"active", jbool(alive(from) && alive(to))}}));
    }
    return jobj({{"nodes", nodes}, {"edges", edges}});
}

} // namespace ergo::studio::plugins::studio
