#include "ergo/package_scan/manifest_reader.h"

#include "ergo/common/json_min.h"
#include "ergo/package_scan/json_lines.h"

namespace ergo::package_scan {

namespace {

namespace jsonm = ergo::common::jsonm;

std::string str(const jsonm::JsonValue& obj, const char* key) {
    const auto* v = obj.find(key);
    return v ? v->as_string() : std::string{};
}

bool fail(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}

void read_observe(const jsonm::JsonValue& doc, const JsonLineIndex& lines, ScanManifest& out) {
    const auto* obs = doc.find("observe");
    if (!obs || !obs->is_object() || !obs->o) return;
    for (const auto& [name, v] : *obs->o) {
        if (!v.is_object()) continue;
        ManifestObserve o{name, str(v, "code"), str(v, "group"), str(v, "path_hint"), {}};
        if (const auto* rels = v.find("relations"); rels && rels->is_array() && rels->a) {
            for (std::size_t i = 0; i < rels->a->size(); ++i) {
                const auto& r = (*rels->a)[i];
                if (!r.is_object()) continue;
                const std::string idx = std::to_string(i);
                o.relations.push_back({str(r, "from"), str(r, "kind"),
                                       lines.line_of({"observe", name, "relations", idx}),
                                       "observe." + name + ".relations[" + idx + "]"});
            }
        }
        out.observe.push_back(std::move(o));
    }
}

void read_ports(const jsonm::JsonValue& doc, const JsonLineIndex& lines, ScanManifest& out) {
    const auto* mods = doc.find("modules");
    if (!mods || !mods->is_object() || !mods->o) return;
    for (const auto& [module, v] : *mods->o) {
        const auto* ports = v.find("ported_from");
        if (!ports || !ports->is_array() || !ports->a) continue;
        for (std::size_t i = 0; i < ports->a->size(); ++i) {
            const auto& p = (*ports->a)[i];
            const std::string package = p.is_object() ? str(p, "package") : p.as_string();
            if (package.empty()) continue;
            const std::string idx = std::to_string(i);
            out.ports.push_back({module, package,
                                 lines.line_of({"modules", module, "ported_from", idx}),
                                 "modules." + module + ".ported_from[" + idx + "]"});
        }
    }
}

void read_tools(const jsonm::JsonValue& doc, ScanManifest& out) {
    const auto* pkgs = doc.find("packages");
    if (!pkgs || !pkgs->is_object() || !pkgs->o) return;
    for (const auto& [name, v] : *pkgs->o) {
        if (!v.is_object() || str(v, "kind") != "tool") continue;
        const std::string path = str(v, "path");
        if (!path.empty()) out.tools.push_back({name, str(v, "code"), path});
    }
}

} // namespace

bool parse_scan_manifest(const std::string& text, ScanManifest& out, std::string* error) {
    out = ScanManifest{};
    jsonm::JsonValue doc;
    if (!jsonm::parse(text, doc) || !doc.is_object()) return fail(error, "manifest: malformed JSON");
    const auto* schema = doc.find("schema");
    if (!schema || !schema->is_number() || schema->n != 1.0) {
        return fail(error, "manifest: unsupported schema (expected 1)");
    }
    // jsonm のオブジェクトは名前順なので、 どの列も名前順に並ぶ。
    const auto lines = JsonLineIndex::build(text);
    read_observe(doc, lines, out);
    read_ports(doc, lines, out);
    read_tools(doc, out);
    return true;
}

} // namespace ergo::package_scan
