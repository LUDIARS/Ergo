#include "ergo/package_scan/package_json_reader.h"

#include "ergo/common/json_min.h"
#include "ergo/package_scan/json_lines.h"

namespace ergo::package_scan {

namespace {

namespace jsonm = ergo::common::jsonm;

bool starts_with(const std::string& s, const char* prefix) {
    return s.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
}

} // namespace

std::string file_spec_path(const std::string& spec) {
    return starts_with(spec, "file:") ? spec.substr(5) : std::string{};
}

bool parse_package_json(const std::string& text, PackageJson& out, std::string* error) {
    out = PackageJson{};
    jsonm::JsonValue doc;
    if (!jsonm::parse(text, doc) || !doc.is_object()) {
        if (error) *error = "malformed package.json";
        return false;
    }
    const auto lines = JsonLineIndex::build(text);
    if (const auto* n = doc.find("name"); n && n->is_string()) {
        out.name      = n->s;
        out.name_line = lines.line_of({"name"});
    }
    for (const char* section : {"dependencies", "devDependencies"}) {
        const auto* deps = doc.find(section);
        if (!deps || !deps->is_object() || !deps->o) continue;
        for (const auto& [name, value] : *deps->o) {
            if (!value.is_string()) continue;
            PackageDep d;
            d.section = section;
            d.name    = name;
            d.spec    = value.s;
            if (starts_with(d.spec, "file:")) {
                d.kind = DepSpecKind::File;
            } else if (starts_with(d.spec, "workspace:")) {
                d.kind = DepSpecKind::Workspace;
            } else if (starts_with(d.name, "@ludiars/")) {
                d.kind = DepSpecKind::Scoped;
            } else {
                continue;
            }
            d.line = lines.line_of({section, name});
            out.deps.push_back(std::move(d));
        }
    }
    return true;
}

} // namespace ergo::package_scan
