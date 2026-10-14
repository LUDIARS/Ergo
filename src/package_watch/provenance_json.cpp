#include "ergo/package_watch/provenance_json.h"

#include "ergo/common/json_min.h"
#include "ergo/package_watch/provenance.h"
#include "json_util.h"

namespace ergo::package_watch {

using detail::get_opt_int;
using detail::get_string;

namespace {

namespace jsonm = ergo::common::jsonm;

/// 文書の key にある配列 (無い・型違いは nullptr と理由)。
const jsonm::JsonValue* top_array(const std::string& text, jsonm::JsonValue& doc, const char* key,
                                  std::string* error) {
    if (!jsonm::parse(text, doc) || !doc.is_object()) {
        if (error) *error = "response is not a JSON object";
        return nullptr;
    }
    const auto* arr = doc.find(key);
    if (!arr || !arr->is_array()) {
        if (error) *error = std::string("response has no '") + key + "' array";
        return nullptr;
    }
    return arr;
}

/// run の対象リポ: セッションの repo_origin (URL) を優先し、 無ければ args.target_repo の末尾名。
std::string run_repository(const jsonm::JsonValue& run) {
    if (const auto* sessions = run.find("sessions"); sessions && sessions->is_array() && sessions->a) {
        for (const auto& s : *sessions->a) {
            const std::string origin = get_string(s, "repo_origin");
            if (!origin.empty()) return repo_key(origin);
        }
    }
    if (const auto* args = run.find("args"); args && args->is_object()) {
        return repo_key(get_string(*args, "target_repo"));
    }
    return {};
}

} // namespace

bool parse_local_prs(const std::string& text, std::vector<LocalPr>& out, std::string* error) {
    out.clear();
    jsonm::JsonValue doc;
    const auto* arr = top_array(text, doc, "pullRequests", error);
    if (!arr) return false;
    if (!arr->a) return true;
    for (const auto& item : *arr->a) {
        if (!item.is_object()) continue;
        const auto number = get_opt_int(item, "number");
        if (!number) continue;
        LocalPr pr;
        pr.number     = *number;
        pr.repository = get_string(item, "repository");
        pr.status     = get_string(item, "status");
        pr.base_ref   = get_string(item, "baseRef");
        pr.head_ref   = get_string(item, "headRef");
        pr.head_sha   = get_string(item, "headSha");
        pr.session_id = get_string(item, "sessionId");
        pr.merge_sha  = get_string(item, "mergeCommitSha");
        out.push_back(std::move(pr));
    }
    return true;
}

bool parse_delegation_runs(const std::string& text, std::vector<DelegationRun>& out, std::string* error) {
    out.clear();
    jsonm::JsonValue doc;
    const auto* arr = top_array(text, doc, "runs", error);
    if (!arr) return false;
    if (!arr->a) return true;
    for (const auto& item : *arr->a) {
        if (!item.is_object()) continue;
        DelegationRun run;
        run.id = get_string(item, "id");
        if (run.id.empty()) continue;
        run.status           = get_string(item, "status");
        run.triggered_by     = get_string(item, "triggered_by");
        run.child_session_id = get_string(item, "child_session_id");
        run.spawn_branch     = get_string(item, "spawn_branch");
        run.repository       = run_repository(item);
        out.push_back(std::move(run));
    }
    return true;
}

} // namespace ergo::package_watch
