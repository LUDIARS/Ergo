#include "ergo/package_model/package_model.h"

namespace ergo::package_model {

const char* to_string(PackageKind kind) {
    switch (kind) {
    case PackageKind::Host:       return "host";
    case PackageKind::Ergo:       return "ergo";
    case PackageKind::Link:       return "link";
    case PackageKind::Tool:       return "tool";
    case PackageKind::ThirdParty: return "third-party";
    case PackageKind::Observe:    return "observe";
    }
    return "unknown";
}

const Package* PackageModel::find_package(const std::string& name) const {
    for (const auto& p : packages) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

const Target* PackageModel::find_target_by_id(const std::string& id) const {
    for (const auto& t : targets) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

const Module* PackageModel::find_module(const std::string& name) const {
    for (const auto& m : modules) {
        if (m.name == name) return &m;
    }
    return nullptr;
}

} // namespace ergo::package_model
