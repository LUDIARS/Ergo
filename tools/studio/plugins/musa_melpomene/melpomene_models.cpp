#include "melpomene_models.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace ergo::studio::plugins::musa_melpomene {
namespace {

std::string lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(" \t\r") - b + 1);
}

/// Non-negative integer (u64 in serde terms).
std::optional<uint64_t> u64(const JsonValue* v) {
    if (!v || !v->is_number() || v->n < 0 || std::floor(v->n) != v->n) return std::nullopt;
    return static_cast<uint64_t>(v->n);
}

std::optional<std::string> str(const JsonValue* v) {
    if (!v || !v->is_string()) return std::nullopt;
    return v->s;
}

std::string str_or(const JsonValue* v, const std::string& dflt) { return str(v).value_or(dflt); }

const JsonValue* path(const JsonValue& v, const char* a, const char* b) {
    const JsonValue* x = field(v, a);
    return x ? field(*x, b) : nullptr;
}

JsonValue opt(const std::optional<std::string>& s) { return s ? jstr(*s) : JsonValue::make_null(); }
JsonValue opt(const std::optional<uint64_t>& n) { return n ? jnum(static_cast<double>(*n)) : JsonValue::make_null(); }
JsonValue strings(const std::vector<std::string>& v) {
    JsonValue a = JsonValue::make_array();
    for (const auto& s : v) a.push(jstr(s));
    return a;
}

struct BodyMeta {
    Priority                             priority = Priority::Medium;
    Category                             category = Category::Bug;
    std::optional<std::string>           scene, object_path;
    std::optional<std::array<double, 3>> world;
};

bool strip(const std::string& line, const char* prefix, std::string& rest) {
    const std::string p(prefix);
    if (line.compare(0, p.size(), p) != 0) return false;
    rest = trim(line.substr(p.size()));
    return true;
}

BodyMeta parse_metadata(const std::string& body) {
    BodyMeta m;
    std::size_t start = 0;
    while (start <= body.size()) {
        const auto end = body.find('\n', start);
        const std::string line = trim(body.substr(start, end == std::string::npos ? std::string::npos : end - start));
        std::string v;
        if (strip(line, "**Priority:**", v)) m.priority = parse_priority(v);
        else if (strip(line, "**Category:**", v)) m.category = parse_category(v);
        else if (strip(line, "**Scene:**", v)) m.scene = v;
        else if (strip(line, "**Object Path:**", v)) m.object_path = v;
        else if (strip(line, "**World Position:**", v)) {
            while (!v.empty() && v.front() == '(') v.erase(0, 1);
            while (!v.empty() && v.back() == ')') v.pop_back();
            std::vector<double> parts;
            std::size_t s = 0;
            while (s <= v.size()) {
                const auto comma = v.find(',', s);
                const std::string p = trim(v.substr(s, comma == std::string::npos ? std::string::npos : comma - s));
                char* e = nullptr;
                const double d = std::strtod(p.c_str(), &e);
                if (!p.empty() && e == p.c_str() + p.size()) parts.push_back(d);
                if (comma == std::string::npos) break;
                s = comma + 1;
            }
            if (parts.size() == 3) m.world = std::array<double, 3>{parts[0], parts[1], parts[2]};
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return m;
}

} // namespace

const char* priority_name(Priority p) {
    switch (p) {
        case Priority::Low: return "Low";
        case Priority::High: return "High";
        case Priority::Critical: return "Critical";
        default: return "Medium";
    }
}

const char* category_name(Category c) {
    switch (c) {
        case Category::Feature: return "Feature";
        case Category::Improvement: return "Improvement";
        case Category::Question: return "Question";
        default: return "Bug";
    }
}

Priority parse_priority(const std::string& s) {
    const std::string l = lower(trim(s));
    if (l == "low") return Priority::Low;
    if (l == "high") return Priority::High;
    if (l == "critical") return Priority::Critical;
    return Priority::Medium;
}

Category parse_category(const std::string& s) {
    const std::string l = lower(trim(s));
    if (l == "feature") return Category::Feature;
    if (l == "improvement") return Category::Improvement;
    if (l == "question") return Category::Question;
    return Category::Bug;
}

std::string Ticket::to_github_body() const {
    std::string b = description + "\n\n---\n";
    if (scene_name) b += "**Scene:** " + *scene_name + "\n";
    if (object_path) b += "**Object Path:** " + *object_path + "\n";
    if (world_position) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "**World Position:** (%.2f, %.2f, %.2f)\n", (*world_position)[0],
                      (*world_position)[1], (*world_position)[2]);
        b += buf;
    }
    b += std::string("**Priority:** ") + priority_name(priority) + "\n";
    b += std::string("**Category:** ") + category_name(category) + "\n";
    return b;
}

double Milestone::progress_percent() const {
    const uint64_t total = open_issues + closed_issues;
    return total == 0 ? 0.0 : static_cast<double>(closed_issues) / static_cast<double>(total) * 100.0;
}

std::optional<Ticket> ticket_from_github(const JsonValue& issue) {
    const auto number = u64(field(issue, "number"));
    const auto title  = str(field(issue, "title"));
    if (!number || !title) return std::nullopt;
    Ticket t;
    t.id                  = "github-" + std::to_string(*number);
    t.github_issue_number = number;
    t.title               = *title;
    t.description         = str_or(field(issue, "body"), "");
    t.state               = str_or(field(issue, "state"), "open");
    t.created_at          = str_or(field(issue, "created_at"), "");
    t.updated_at          = str_or(field(issue, "updated_at"), "");
    t.author              = str(path(issue, "user", "login"));
    if (const JsonValue* labels = field(issue, "labels"); labels && labels->is_array()) {
        for (const auto& l : *labels->a) {
            if (auto n = str(field(l, "name"))) t.labels.push_back(*n);
        }
    }
    if (const JsonValue* as = field(issue, "assignees"); as && as->is_array()) {
        for (const auto& a : *as->a) {
            if (auto n = str(field(a, "login"))) t.assignees.push_back(*n);
        }
    }
    t.milestone_id = u64(path(issue, "milestone", "number"));
    const BodyMeta m = parse_metadata(t.description);
    t.priority       = m.priority;
    t.category       = m.category;
    t.scene_name     = m.scene;
    t.object_path    = m.object_path;
    t.world_position = m.world;
    return t;
}

std::optional<Comment> comment_from_github(const JsonValue& v) {
    const auto id   = u64(field(v, "id"));
    const auto body = str(field(v, "body"));
    if (!id || !body) return std::nullopt;
    return Comment{*id, *body, str_or(path(v, "user", "login"), ""), str_or(field(v, "created_at"), ""),
                   str_or(field(v, "updated_at"), "")};
}

std::optional<Milestone> milestone_from_github(const JsonValue& v) {
    const auto number = u64(field(v, "number"));
    const auto title  = str(field(v, "title"));
    if (!number || !title) return std::nullopt;
    Milestone m;
    m.number        = *number;
    m.title         = *title;
    m.description   = str(field(v, "description"));
    m.state         = str_or(field(v, "state"), "open");
    m.open_issues   = u64(field(v, "open_issues")).value_or(0);
    m.closed_issues = u64(field(v, "closed_issues")).value_or(0);
    m.due_on        = str(field(v, "due_on"));
    m.created_at    = str_or(field(v, "created_at"), "");
    m.updated_at    = str_or(field(v, "updated_at"), "");
    return m;
}

std::optional<PullRequest> pull_request_from_github(const JsonValue& v) {
    const auto number = u64(field(v, "number"));
    const auto title  = str(field(v, "title"));
    if (!number || !title) return std::nullopt;
    PullRequest p;
    p.number   = *number;
    p.title    = *title;
    p.state    = str_or(field(v, "state"), "open");
    p.html_url = str_or(field(v, "html_url"), "");
    p.head_ref = str_or(path(v, "head", "ref"), "");
    p.base_ref = str_or(path(v, "base", "ref"), "");
    if (const JsonValue* mg = field(v, "mergeable"); mg && mg->is_bool()) p.mergeable = mg->b;
    p.user       = str_or(path(v, "user", "login"), "");
    p.created_at = str_or(field(v, "created_at"), "");
    p.updated_at = str_or(field(v, "updated_at"), "");
    return p;
}

std::optional<Review> review_from_github(const JsonValue& v) {
    const auto id = u64(field(v, "id"));
    if (!id) return std::nullopt;
    return Review{*id, str_or(field(v, "state"), ""), str(field(v, "body")), str_or(path(v, "user", "login"), ""),
                  str_or(field(v, "submitted_at"), "")};
}

std::optional<WorkflowRun> workflow_run_from_github(const JsonValue& v) {
    const auto id   = u64(field(v, "id"));
    const auto name = str(field(v, "name"));
    if (!id || !name) return std::nullopt;
    WorkflowRun w;
    w.id          = *id;
    w.name        = *name;
    w.status      = str_or(field(v, "status"), "");
    w.conclusion  = str(field(v, "conclusion"));
    w.html_url    = str_or(field(v, "html_url"), "");
    w.head_branch = str_or(field(v, "head_branch"), "");
    w.created_at  = str_or(field(v, "created_at"), "");
    w.updated_at  = str_or(field(v, "updated_at"), "");
    return w;
}

JsonValue to_json_value(const Comment& c) {
    return jobj({{"id", jnum(static_cast<double>(c.id))}, {"body", jstr(c.body)}, {"user", jstr(c.user)},
                 {"created_at", jstr(c.created_at)}, {"updated_at", jstr(c.updated_at)}});
}

JsonValue to_json_value(const Ticket& t) {
    JsonValue world = JsonValue::make_null(), screen = JsonValue::make_null();
    if (t.world_position) {
        world = JsonValue::make_array();
        for (double d : *t.world_position) world.push(jnum(d));
    }
    if (t.screen_position) {
        screen = JsonValue::make_array();
        for (double d : *t.screen_position) screen.push(jnum(d));
    }
    return jobj({{"id", jstr(t.id)}, {"github_issue_number", opt(t.github_issue_number)}, {"title", jstr(t.title)},
                 {"description", jstr(t.description)}, {"priority", jstr(lower(priority_name(t.priority)))},
                 {"category", jstr(lower(category_name(t.category)))}, {"labels", strings(t.labels)},
                 {"milestone_id", opt(t.milestone_id)}, {"scene_name", opt(t.scene_name)},
                 {"object_path", opt(t.object_path)}, {"world_position", world}, {"screen_position", screen},
                 {"created_at", jstr(t.created_at)}, {"updated_at", jstr(t.updated_at)}, {"state", jstr(t.state)},
                 {"author", opt(t.author)}, {"assignees", strings(t.assignees)}, {"comments", to_json_array(t.comments)}});
}

JsonValue to_json_value(const Milestone& m) {
    return jobj({{"number", jnum(static_cast<double>(m.number))}, {"title", jstr(m.title)},
                 {"description", opt(m.description)}, {"state", jstr(m.state)},
                 {"open_issues", jnum(static_cast<double>(m.open_issues))},
                 {"closed_issues", jnum(static_cast<double>(m.closed_issues))}, {"due_on", opt(m.due_on)},
                 {"created_at", jstr(m.created_at)}, {"updated_at", jstr(m.updated_at)},
                 {"progress_percent", jnum(m.progress_percent())}});
}

JsonValue to_json_value(const Review& r) {
    return jobj({{"id", jnum(static_cast<double>(r.id))}, {"state", jstr(r.state)}, {"body", opt(r.body)},
                 {"user", jstr(r.user)}, {"submitted_at", jstr(r.submitted_at)}});
}

JsonValue to_json_value(const PullRequest& p) {
    return jobj({{"number", jnum(static_cast<double>(p.number))}, {"title", jstr(p.title)}, {"state", jstr(p.state)},
                 {"html_url", jstr(p.html_url)}, {"head_ref", jstr(p.head_ref)}, {"base_ref", jstr(p.base_ref)},
                 {"mergeable", p.mergeable ? jbool(*p.mergeable) : JsonValue::make_null()}, {"user", jstr(p.user)},
                 {"created_at", jstr(p.created_at)}, {"updated_at", jstr(p.updated_at)},
                 {"review_count", jnum(static_cast<double>(p.review_count))}, {"reviews", to_json_array(p.reviews)}});
}

JsonValue to_json_value(const WorkflowRun& w) {
    return jobj({{"id", jnum(static_cast<double>(w.id))}, {"name", jstr(w.name)}, {"status", jstr(w.status)},
                 {"conclusion", opt(w.conclusion)}, {"html_url", jstr(w.html_url)}, {"head_branch", jstr(w.head_branch)},
                 {"created_at", jstr(w.created_at)}, {"updated_at", jstr(w.updated_at)}});
}

} // namespace ergo::studio::plugins::musa_melpomene
