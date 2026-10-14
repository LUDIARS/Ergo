#include "ergo/package_model/declaration_check.h"

#include <algorithm>
#include <tuple>

namespace ergo::package_model {

namespace {

bool contains(const std::vector<std::string>& list, const std::string& value) {
    return std::find(list.begin(), list.end(), value) != list.end();
}

bool declared(const PackageModel& model, const Target& from, const Package& from_pkg,
              const std::string& to_package, std::string& module) {
    if (from_pkg.kind == PackageKind::Link) return contains(from_pkg.requires_, to_package);
    module = module_for_target(model, from.name);
    if (module.empty()) return false;
    const Module* m = model.find_module(module);
    return m && (contains(m->needs, to_package) || contains(m->optional, to_package));
}

} // namespace

std::string module_for_target(const PackageModel& model, const std::string& name) {
    std::string best;
    for (const auto& m : model.modules) {
        const bool hit = name == m.name ||
                         (name.size() > m.name.size() + 1 &&
                          name.compare(0, m.name.size(), m.name) == 0 && name[m.name.size()] == '_');
        if (hit && m.name.size() > best.size()) best = m.name;
    }
    return best;
}

std::vector<UndeclaredLink> find_undeclared_links(const PackageModel& model, bool include_excluded) {
    std::vector<UndeclaredLink> out;
    for (const auto& t : model.targets) {
        if (t.excluded && !include_excluded) continue;
        const Package* from_pkg = model.find_package(t.package);
        if (!from_pkg ||
            (from_pkg->kind != PackageKind::Ergo && from_pkg->kind != PackageKind::Link)) {
            continue;
        }
        for (const auto& dep_id : t.dependencies) {
            const Target* dep = model.find_target_by_id(dep_id);
            if (!dep || dep->package == t.package) continue;
            if (dep->excluded && !include_excluded) continue;
            const Package* to_pkg = model.find_package(dep->package);
            if (!to_pkg || to_pkg->kind != PackageKind::Link) continue;
            std::string module;
            if (declared(model, t, *from_pkg, dep->package, module)) continue;
            out.push_back({t.package, t.name, dep->package, dep->name, module});
        }
    }
    std::sort(out.begin(), out.end(), [](const UndeclaredLink& a, const UndeclaredLink& b) {
        return std::tie(a.from_target, a.to_target) < std::tie(b.from_target, b.to_target);
    });
    out.erase(std::unique(out.begin(), out.end(),
                          [](const UndeclaredLink& a, const UndeclaredLink& b) {
                              return a.from_target == b.from_target && a.to_target == b.to_target;
                          }),
              out.end());
    return out;
}

std::string describe(const UndeclaredLink& l) {
    std::string where = l.module.empty() ? "no module declares it"
                                         : "module " + l.module + " does not list " + l.to_package;
    return "PKG-17 " + l.from_package + ":" + l.from_target + " -> " + l.to_package + ":" +
           l.to_target + " (" + where + ")";
}

} // namespace ergo::package_model
