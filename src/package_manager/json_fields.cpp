#include "json_fields.h"

namespace ergo::package_manager::detail {

void manifest_error(std::vector<Finding>& findings, const std::string& subject,
                    const std::string& message) {
    findings.push_back({"MANIFEST", Severity::Error, subject, message});
}

std::string string_field(const jsonm::JsonValue& obj, const std::string& key,
                         const std::string& where, std::vector<Finding>& findings) {
    const auto* v = obj.find(key);
    if (!v || v->is_null()) return {};
    if (!v->is_string()) {
        manifest_error(findings, where, key + " must be a string");
        return {};
    }
    return v->s;
}

std::vector<std::string> string_array_field(const jsonm::JsonValue& obj, const std::string& key,
                                            const std::string& where,
                                            std::vector<Finding>& findings) {
    std::vector<std::string> out;
    const auto* v = obj.find(key);
    if (!v || v->is_null()) return out;
    if (!v->is_array() || !v->a) {
        manifest_error(findings, where, key + " must be an array of strings");
        return out;
    }
    for (const auto& item : *v->a) {
        if (!item.is_string()) {
            manifest_error(findings, where, key + " must contain only strings");
            continue;
        }
        out.push_back(item.s);
    }
    return out;
}

std::map<std::string, std::string> string_map_field(const jsonm::JsonValue& obj,
                                                    const std::string& key,
                                                    const std::string& where,
                                                    std::vector<Finding>& findings) {
    std::map<std::string, std::string> out;
    const auto* v = obj.find(key);
    if (!v || v->is_null()) return out;
    if (!v->is_object() || !v->o) {
        manifest_error(findings, where, key + " must be an object of strings");
        return out;
    }
    for (const auto& [k, item] : *v->o) {
        if (!item.is_string()) {
            manifest_error(findings, where, key + "." + k + " must be a string");
            continue;
        }
        out[k] = item.s;
    }
    return out;
}

} // namespace ergo::package_manager::detail
