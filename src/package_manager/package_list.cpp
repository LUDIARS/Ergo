#include "ergo/package_manager/package_list.h"

#include <algorithm>

namespace ergo::package_manager {

namespace {

bool contains(const std::vector<std::string>& v, const std::string& x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

std::string resolved_field(const jsonm::JsonValue* resolved, const std::string& name,
                           const char* key) {
    if (!resolved) return {};
    const auto* pkgs = resolved->find("packages");
    const auto* pkg  = pkgs ? pkgs->find(name) : nullptr;
    const auto* v    = pkg ? pkg->find(key) : nullptr;
    return (v && v->is_string()) ? v->s : std::string();
}

ListRow package_row(const ManifestPackage& p, const char* owner, const ListInput& in) {
    ListRow r;
    r.name       = p.name;
    r.owner      = owner;
    r.kind       = p.kind;
    r.code       = p.code;
    r.rev        = p.rev;
    r.expect_tag = p.expect_tag;
    if (in.host) {
        r.used     = contains(in.host->use, p.name);
        r.optional = contains(in.host->optional, p.name);
        const auto f = in.host->features.find(p.name);
        if (f != in.host->features.end()) r.features = f->second;
    }
    r.route = resolved_field(in.resolved, p.name, "route");
    r.state = resolved_field(in.resolved, p.name, "state");
    return r;
}

std::string pad(const std::string& s, std::size_t width) {
    return s.size() >= width ? s + " " : s + std::string(width - s.size() + 1, ' ');
}

std::string short_rev(const std::string& rev) {
    return rev.size() > 12 ? rev.substr(0, 12) : rev;
}

} // namespace

std::vector<ListRow> make_list(const ListInput& in) {
    std::vector<ListRow> rows;
    if (in.host && in.host->ergo.present) {
        ListRow r;
        r.name       = "ergo";
        r.owner      = "host";
        r.kind       = "ergo";
        r.code       = "Eg";
        r.rev        = in.host->ergo.rev;
        r.expect_tag = in.host->ergo.expect_tag;
        r.used       = true;
        rows.push_back(r);
    }
    if (in.ergo) {
        for (const auto& p : in.ergo->packages) rows.push_back(package_row(p, "ergo", in));
    }
    if (in.host) {
        for (const auto& p : in.host->packages) rows.push_back(package_row(p, "host", in));
    }
    if (in.upm) {
        for (const auto& d : in.upm->dependencies) {
            ListRow r;
            r.name  = d.name;
            r.owner = "upm";
            r.kind  = "upm";
            r.rev   = d.rev;
            r.used  = true;
            r.state = d.branch_ref ? "branch-ref" : "";
            rows.push_back(r);
        }
    }
    return rows;
}

std::string render_list_text(const std::vector<ListRow>& rows) {
    struct Col { const char* head; std::size_t width; };
    const Col cols[] = {{"name", 4}, {"owner", 5}, {"kind", 4}, {"code", 4}, {"rev", 12},
                        {"tag", 3}, {"use", 3}, {"features", 8}, {"route", 5}, {"state", 5}};
    std::vector<std::vector<std::string>> cells;
    for (const auto& r : rows) {
        std::string use = r.used ? (r.optional ? "opt" : "yes") : "-";
        cells.push_back({r.name, r.owner, r.kind, r.code, short_rev(r.rev), r.expect_tag, use,
                         r.features, r.route, r.state});
    }
    std::vector<std::size_t> width;
    for (const auto& c : cols) width.push_back(c.width);
    for (const auto& line : cells) {
        for (std::size_t i = 0; i < line.size(); ++i) width[i] = std::max(width[i], line[i].size());
    }
    auto emit = [&](const std::vector<std::string>& line) {
        std::string out;
        for (std::size_t i = 0; i < line.size(); ++i) {
            out += (i + 1 == line.size()) ? line[i] : pad(line[i], width[i]);
        }
        while (!out.empty() && out.back() == ' ') out.pop_back();
        return out + "\n";
    };
    std::vector<std::string> head;
    for (const auto& c : cols) head.push_back(c.head);
    std::string out = emit(head);
    for (const auto& line : cells) out += emit(line);
    return out;
}

jsonm::JsonValue list_to_json(const std::vector<ListRow>& rows, const std::string& host_code) {
    auto doc = jsonm::JsonValue::make_object();
    doc.set("host", host_code.empty() ? jsonm::JsonValue::make_null()
                                      : jsonm::JsonValue::make_string(host_code));
    auto arr = jsonm::JsonValue::make_array();
    for (const auto& r : rows) {
        auto o = jsonm::JsonValue::make_object();
        o.set("name", jsonm::JsonValue::make_string(r.name));
        o.set("owner", jsonm::JsonValue::make_string(r.owner));
        o.set("kind", jsonm::JsonValue::make_string(r.kind));
        o.set("code", jsonm::JsonValue::make_string(r.code));
        o.set("rev", jsonm::JsonValue::make_string(r.rev));
        o.set("expect_tag", jsonm::JsonValue::make_string(r.expect_tag));
        o.set("used", jsonm::JsonValue::make_bool(r.used));
        o.set("optional", jsonm::JsonValue::make_bool(r.optional));
        o.set("features", jsonm::JsonValue::make_string(r.features));
        o.set("route", jsonm::JsonValue::make_string(r.route));
        o.set("state", jsonm::JsonValue::make_string(r.state));
        arr.push(std::move(o));
    }
    doc.set("packages", std::move(arr));
    return doc;
}

} // namespace ergo::package_manager
