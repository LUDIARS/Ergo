#include "ergo/studio/project/recent_projects.h"

#include "ergo/common/json_min.h"

#include <algorithm>

namespace ergo::studio::project {

namespace jsonm = ergo::common::jsonm;

namespace {

constexpr double kVersion = 1;

std::string text_of(const jsonm::JsonValue& v, const char* key) {
    const jsonm::JsonValue* f = v.find(key);
    return f != nullptr ? f->as_string() : std::string();
}

} // namespace

std::optional<std::vector<RecentProject>> parse_recent_projects(const std::string& text, std::string& err) {
    jsonm::JsonValue doc;
    if (!jsonm::parse(text, doc) || !doc.is_object()) {
        err = "not a JSON object";
        return std::nullopt;
    }
    const jsonm::JsonValue* version = doc.find("version");
    if (version == nullptr || !version->is_number() || version->n != kVersion) {
        err = "unknown version (expected 1)";
        return std::nullopt;
    }
    const jsonm::JsonValue* projects = doc.find("projects");
    if (projects == nullptr || !projects->is_array()) {
        err = "\"projects\" is not an array";
        return std::nullopt;
    }
    std::vector<RecentProject> out;
    for (const jsonm::JsonValue& item : *projects->a) {
        if (!item.is_object()) {
            err = "a project is not an object";
            return std::nullopt;
        }
        RecentProject p{text_of(item, "source"), text_of(item, "name"),   text_of(item, "root"),
                        text_of(item, "style"),  text_of(item, "domain"), text_of(item, "last_opened")};
        if (p.source.empty()) {
            err = "a project has no \"source\"";
            return std::nullopt;
        }
        const bool seen = std::any_of(out.begin(), out.end(), [&](const RecentProject& r) { return r.source == p.source; });
        if (!seen && out.size() < kMaxRecentProjects) out.push_back(std::move(p));
    }
    return out;
}

std::string serialize_recent_projects(const std::vector<RecentProject>& list) {
    jsonm::JsonValue projects = jsonm::JsonValue::make_array();
    for (const RecentProject& p : list) {
        jsonm::JsonValue item = jsonm::JsonValue::make_object();
        item.set("source", jsonm::JsonValue::make_string(p.source));
        item.set("name", jsonm::JsonValue::make_string(p.name));
        item.set("root", jsonm::JsonValue::make_string(p.root));
        item.set("style", jsonm::JsonValue::make_string(p.style));
        item.set("domain", jsonm::JsonValue::make_string(p.domain));
        item.set("last_opened", jsonm::JsonValue::make_string(p.last_opened));
        projects.push(std::move(item));
    }
    jsonm::JsonValue doc = jsonm::JsonValue::make_object();
    doc.set("version", jsonm::JsonValue::make_number(kVersion));
    doc.set("projects", std::move(projects));
    return jsonm::serialize(doc);
}

void remember_project(std::vector<RecentProject>& list, RecentProject p, std::size_t max) {
    forget_project(list, p.source);
    list.insert(list.begin(), std::move(p));
    if (list.size() > max) list.resize(max);
}

bool forget_project(std::vector<RecentProject>& list, const std::string& source) {
    const auto it = std::remove_if(list.begin(), list.end(), [&](const RecentProject& r) { return r.source == source; });
    const bool found = it != list.end();
    list.erase(it, list.end());
    return found;
}

} // namespace ergo::studio::project
