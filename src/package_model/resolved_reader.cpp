#include "ergo/package_model/resolved_reader.h"

#include "json_access.h"

#include <algorithm>
#include <utility>

namespace ergo::package_model {

namespace {

using detail::get_bool;
using detail::get_string;
using detail::get_strings;
using detail::jsonm::JsonValue;

bool fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

Package read_package(const std::string& name, const JsonValue& v) {
    Package p;
    p.name  = name;
    p.code  = get_string(v, "code");
    p.group = get_string(v, "group");
    p.state = get_string(v, "state");
    if (get_string(v, "kind") == "tool") {
        p.kind        = PackageKind::Tool;
        p.path        = get_string(v, "path");
        p.gitlink     = get_string(v, "gitlink");
        p.initialized = get_bool(v, "initialized");
        return p;
    }
    p.kind            = PackageKind::Link;
    p.pin             = get_string(v, "pin");
    p.expect_tag      = get_string(v, "expect_tag");
    p.rev_override    = get_string(v, "rev_override");
    p.source_dir      = get_string(v, "source_dir");
    p.targets_present = get_strings(v, "targets_present");
    p.features        = get_strings(v, "features");
    p.requires_       = get_strings(v, "requires");
    p.cmake_version   = get_string(v, "version");
    p.route           = get_string(v, "route");
    p.mode            = get_string(v, "mode");
    p.head            = get_string(v, "head");
    p.drift           = get_string(v, "drift");
    p.dirty           = get_bool(v, "dirty");
    if (!p.cmake_version.empty()) p.version = {p.cmake_version, "cmake"};
    return p;
}

ObserveEntry read_observe(const std::string& name, const JsonValue& v) {
    ObserveEntry o;
    o.name      = name;
    o.code      = get_string(v, "code");
    o.group     = get_string(v, "group");
    o.path_hint = get_string(v, "path_hint");
    for (const auto& r : get_strings(v, "relations")) {
        const auto cut = r.find(':');
        if (cut == std::string::npos) continue;
        o.relations.push_back({r.substr(0, cut), r.substr(cut + 1)});
    }
    return o;
}

} // namespace

bool read_resolved_text(const std::string& json, PackageModel& model, std::string* error) {
    JsonValue doc;
    if (!detail::jsonm::parse(json, doc) || !doc.is_object()) {
        return fail(error, "resolved.json: malformed JSON");
    }
    const auto* schema = doc.find("schema");
    if (!schema || !schema->is_number() || schema->n != 1.0) {
        return fail(error, "resolved.json: unsupported schema (expected 1)");
    }
    model.mode     = get_string(doc, "mode");
    model.strict   = get_bool(doc, "strict");
    model.platform = get_string(doc, "platform");
    model.packages.clear();
    model.modules.clear();
    model.observe.clear();
    model.findings.clear();

    if (const auto* pkgs = detail::get_object(doc, "packages")) {
        for (const auto& [name, v] : *pkgs->o) {
            if (v.is_object()) model.packages.push_back(read_package(name, v));
        }
    }
    if (const auto* mods = detail::get_object(doc, "modules")) {
        for (const auto& [name, v] : *mods->o) {
            if (!v.is_object()) continue;
            Module m;
            m.name        = name;
            m.enabled     = get_bool(v, "enabled");
            m.needs       = get_strings(v, "needs");
            m.optional    = get_strings(v, "optional");
            m.ported_from = get_strings(v, "ported_from");
            m.reason      = get_string(v, "reason");
            model.modules.push_back(std::move(m));
        }
    }
    if (const auto* obs = detail::get_object(doc, "observe")) {
        for (const auto& [name, v] : *obs->o) {
            if (!v.is_object()) continue;
            auto entry = read_observe(name, v);
            Package p;
            p.name  = entry.name;
            p.code  = entry.code;
            p.group = entry.group;
            p.kind  = PackageKind::Observe;
            model.packages.push_back(std::move(p));
            model.observe.push_back(std::move(entry));
        }
    }
    if (const auto* findings = detail::get_array(doc, "findings")) {
        for (const auto& f : *findings) {
            if (!f.is_object()) continue;
            model.findings.push_back({get_string(f, "id"), get_string(f, "severity"),
                                      get_string(f, "subject"), get_string(f, "message")});
        }
    }
    // jsonm のオブジェクトは名前順に並ぶが、 observe を足したので並べ直す。
    std::sort(model.packages.begin(), model.packages.end(),
              [](const Package& a, const Package& b) { return a.name < b.name; });
    return true;
}

bool read_resolved_file(const std::string& path, PackageModel& model, std::string* error) {
    std::string text;
    if (!detail::read_text_file(path, text)) return fail(error, "cannot read " + path);
    return read_resolved_text(text, model, error);
}

} // namespace ergo::package_model
