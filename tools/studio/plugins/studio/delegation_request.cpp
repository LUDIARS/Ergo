#include "delegation_request.h"

#include <algorithm>
#include <cctype>
#include <regex>

namespace ergo::studio::plugins::studio {
namespace {

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

std::size_t code_points(const std::string& s) {
    std::size_t n = 0;
    for (unsigned char c : s) {
        if ((c & 0xC0) != 0x80) ++n;
    }
    return n;
}

std::string str_of(const JsonValue& v, const char* k) {
    const JsonValue* x = field(v, k);
    return x && x->is_string() ? x->s : std::string();
}

std::string norm_path(std::string p) {
    std::replace(p.begin(), p.end(), '\\', '/');
    while (!p.empty() && p.back() == '/') p.pop_back();
    for (char& c : p) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return p;
}

} // namespace

BuildResult build_invoke_request(const JsonValue& project, const JsonValue& tpl, const InstructionInput& in,
                                 const std::string& triggered_by) {
    BuildResult out;
    const std::string call_name = str_of(tpl, "call_name");
    if (call_name != in.call_name) {
        out.reason = "テンプレートが一致しません";
        return out;
    }
    const std::string repo_path = str_of(project, "repo_path");
    const std::string task_text = trim(in.task_text);
    const JsonValue*  schema    = field(tpl, "input_schema");
    bool task_in_schema = false;
    JsonValue args = JsonValue::make_object();
    if (schema && schema->is_array()) {
        for (const auto& f : *schema->a) {
            if (str_of(f, "name") == "task") task_in_schema = true;
        }
        for (const auto& f : *schema->a) {
            const std::string name = str_of(f, "name");
            if (name.empty()) continue;
            const JsonValue* raw = field(in.args, name);
            JsonValue value = raw ? *raw : JsonValue::make_null();
            const bool empty_in = !raw || (raw->is_string() && raw->s.empty());
            if (name == "task" && !task_text.empty() && empty_in) value = jstr(task_text);
            if (value.is_string()) value = jstr(trim(value.s));
            const bool empty = value.is_null() || (value.is_string() && value.s.empty());
            if (empty && name == "target_repo") value = jstr(repo_path);
            if (value.is_null() || (value.is_string() && value.s.empty())) {
                const JsonValue* req = field(f, "required");
                if (req && req->as_bool()) {
                    out.reason = "必須の入力「" + name + "」が空です";
                    return out;
                }
                continue;
            }
            args.set(name, value);
        }
    }
    std::string extra;
    if (!task_text.empty() && !task_in_schema) extra = task_text;
    const std::string ep = trim(in.extra_prompt);
    if (!ep.empty()) extra += (extra.empty() ? "" : "\n\n") + ep;
    if (code_points(extra) > kMaxExtraPrompt) {
        out.reason = "追加指示は " + std::to_string(kMaxExtraPrompt) + " 文字までです";
        return out;
    }
    JsonValue request = jobj({{"call_name", jstr(call_name)}, {"args", args}, {"cwd", jstr(repo_path)},
                              {"triggered_by", jstr(triggered_by)}, {"project", jstr(str_of(project, "code"))}});
    if (!extra.empty()) request.set("extra_prompt", jstr(extra));
    if (in.actio_task_id) {
        static const std::regex id(R"(^[0-9a-zA-Z][0-9a-zA-Z_-]{0,127}$)");
        if (!std::regex_match(*in.actio_task_id, id)) {
            out.reason = "Actio のタスク ID が不正です";
            return out;
        }
        request.set("taskflow_reference", jstr("actio:" + *in.actio_task_id));
    }
    out.ok      = true;
    out.request = std::move(request);
    return out;
}

std::string task_prompt(const std::string& id, const std::string& title, const std::string& description) {
    const std::string body = trim(description);
    return body.empty() ? title + "\n\n(Actio: actio:" + id + ")" : title + "\n\n" + body + "\n\n(Actio: actio:" + id + ")";
}

bool run_belongs_to(const JsonValue& run, const std::string& repo_path) {
    const JsonValue* cwd = field(run, "cwd");
    if (cwd && cwd->is_string() && norm_path(cwd->s) == norm_path(repo_path)) return true;
    const JsonValue* args   = field(run, "args");
    const JsonValue* target = args ? field(*args, "target_repo") : nullptr;
    return target && target->is_string() && norm_path(target->s) == norm_path(repo_path);
}

std::optional<std::string> repo_slug_from_origin(const std::string& origin) {
    static const std::regex re(R"(github\.com[:/]([^/\s]+)/([^/\s]+?)(?:\.git)?/?$)", std::regex::icase);
    std::smatch m;
    const std::string o = trim(origin);
    if (!std::regex_search(o, m, re)) return std::nullopt;
    return m[1].str() + "/" + m[2].str();
}

} // namespace ergo::studio::plugins::studio
