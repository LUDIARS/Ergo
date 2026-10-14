#include "ergo/package_manager/package_owner.h"

namespace ergo::package_manager {

const char* const kErgoPackageName = "ergo";

const char* owner_name(PackageOwner owner) {
    switch (owner) {
        case PackageOwner::HostErgo: return "host (host.ergo)";
        case PackageOwner::HostPrivate: return "host (private packages)";
        case PackageOwner::ErgoShared: return "Ergo manifest";
        case PackageOwner::Conflict: return "conflict (PKG-15)";
        case PackageOwner::Unknown: break;
    }
    return "unknown";
}

PackageOwner owner_of(const std::string& name, const HostDeclaration* host, const ErgoManifest* ergo) {
    if (name == kErgoPackageName) return PackageOwner::HostErgo;
    bool is_private = false;
    if (host) {
        for (const auto& p : host->packages) {
            if (p.name == name) is_private = true;
        }
    }
    const bool is_shared = ergo && ergo->find(name) != nullptr;
    if (is_private && ergo && ergo->has_name(name)) return PackageOwner::Conflict;
    if (is_private) return PackageOwner::HostPrivate;
    if (is_shared) return PackageOwner::ErgoShared;
    return PackageOwner::Unknown;
}

JsonPath pin_path(PackageOwner owner, const std::string& name) {
    if (owner == PackageOwner::HostErgo) return {"host", "ergo"};
    return {"packages", name};
}

} // namespace ergo::package_manager
