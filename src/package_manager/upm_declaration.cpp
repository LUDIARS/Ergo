#include "ergo/package_manager/upm_declaration.h"

#include "ergo/package_manager/declaration_check.h"

namespace ergo::package_manager {

std::string read_upm_host_code(const jsonm::JsonValue& doc) {
    const auto* code = doc.find("code");
    return (code && code->is_string()) ? code->s : std::string();
}

std::vector<UpmDependency> read_upm_dependencies(const jsonm::JsonValue& manifest) {
    std::vector<UpmDependency> out;
    const auto* deps = manifest.find("dependencies");
    if (!deps || !deps->is_object() || !deps->o) return out;
    for (const auto& [name, value] : *deps->o) {
        if (!value.is_string() || value.s.find("LUDIARS/") == std::string::npos) continue;
        UpmDependency d;
        d.name = name;
        const std::size_t hash = value.s.find('#');
        d.url = value.s.substr(0, hash);
        if (hash != std::string::npos) d.rev = value.s.substr(hash + 1);
        d.branch_ref = !is_full_sha(d.rev);
        out.push_back(std::move(d));
    }
    return out;
}

} // namespace ergo::package_manager
