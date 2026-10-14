#include "json_access.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace ergo::package_model::detail {

std::string get_string(const jsonm::JsonValue& obj, const std::string& key) {
    const auto* v = obj.find(key);
    return v ? v->as_string() : std::string{};
}

bool get_bool(const jsonm::JsonValue& obj, const std::string& key) {
    const auto* v = obj.find(key);
    return v ? v->as_bool() : false;
}

std::vector<std::string> get_strings(const jsonm::JsonValue& obj, const std::string& key) {
    std::vector<std::string> out;
    const auto* v = obj.find(key);
    if (!v || !v->is_array() || !v->a) return out;
    for (const auto& item : *v->a) {
        if (item.is_string()) out.push_back(item.s);
    }
    return out;
}

const jsonm::JsonValue* get_object(const jsonm::JsonValue& obj, const std::string& key) {
    const auto* v = obj.find(key);
    return (v && v->is_object() && v->o) ? v : nullptr;
}

const jsonm::JsonArray* get_array(const jsonm::JsonValue& obj, const std::string& key) {
    const auto* v = obj.find(key);
    return (v && v->is_array() && v->a) ? v->a.get() : nullptr;
}

std::string generic_utf8(const std::filesystem::path& path) {
    const auto s = path.generic_u8string();
    return std::string(s.begin(), s.end());
}

bool read_text_file(const std::string& path, std::string& out) {
    std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
    if (!file) return false;
    std::ostringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
}

bool parse_json_file(const std::string& path, jsonm::JsonValue& out, std::string* error) {
    std::string text;
    if (!read_text_file(path, text)) {
        if (error) *error = "cannot read " + path;
        return false;
    }
    if (!jsonm::parse(text, out)) {
        if (error) *error = "malformed JSON: " + path;
        return false;
    }
    return true;
}

} // namespace ergo::package_model::detail
