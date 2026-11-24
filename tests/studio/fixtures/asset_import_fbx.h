#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace studio_test {
namespace asset_fbx {

inline void little_endian(std::string& out, uint64_t value, unsigned width) {
    for (unsigned byte = 0; byte < width; ++byte) {
        out.push_back(static_cast<char>((value >> (8 * byte)) & 0xff));
    }
}

inline std::string id(uint64_t value) {
    std::string result = "L";
    little_endian(result, value, 8);
    return result;
}

inline std::string text(const std::string& value) {
    std::string result = "S";
    little_endian(result, value.size(), 4);
    return result + value;
}

inline std::string array(char tag, std::initializer_list<uint32_t> words) {
    std::string result(1, tag);
    little_endian(result, words.size(), 4);
    little_endian(result, 0, 4);   // uncompressed
    little_endian(result, words.size() * 4, 4);
    for (const auto word : words) little_endian(result, word, 4);
    return result;
}

struct Node {
    Node(std::string key, std::vector<std::string> values = {}, std::vector<Node> nodes = {})
        : name(std::move(key)), properties(std::move(values)), children(std::move(nodes)) {}

    std::string name;
    std::vector<std::string> properties;
    std::vector<Node> children;

    void encode(std::string& out) const {
        const auto start = out.size();
        out.append(4, '\0');  // absolute end offset, patched after the children
        little_endian(out, properties.size(), 4);
        std::string bytes;
        for (const auto& p : properties) bytes += p;
        little_endian(out, bytes.size(), 4);
        little_endian(out, name.size(), 1);
        out += name;
        out += bytes;
        for (const auto& child : children) child.encode(out);
        if (!children.empty()) out.append(13, '\0');
        std::string end;
        little_endian(end, out.size(), 4);
        out.replace(start, 4, end);
    }
};

// Original FBX 7.4 fixture generated for this test. The triangle has one
// material, one relative diffuse texture and one unbound animation stack.
inline std::string triangle() {
    std::string out("Kaydara FBX Binary  \0\x1a\0", 23);
    little_endian(out, 7400, 4);
    Node("Objects", {}, {
        {"Geometry", {id(1), text("Geometry::Triangle"), text("Mesh")}, {
            {"Vertices", {array('f', {0, 0, 0, 0x40000000, 0, 0, 0, 0x40400000, 0})}},
            {"PolygonVertexIndex", {array('i', {0, 1, static_cast<uint32_t>(-3)})}},
        }},
        {"Model", {id(2), text("Model::Triangle"), text("Mesh")}},
        {"Material", {id(3), text("Material::Paint"), text("")}, {
            {"ShadingModel", {text("lambert")}},
        }},
        {"Texture", {id(4), text("Texture::Paint"), text("")}, {
            {"RelativeFilename", {text("paint.png")}},
        }},
        {"AnimationStack", {id(5), text("AnimStack::Idle"), text("")}},
    }).encode(out);
    Node("Connections", {}, {
        {"C", {text("OO"), id(1), id(2)}},
        {"C", {text("OO"), id(2), id(0)}},
        {"C", {text("OO"), id(3), id(2)}},
        {"C", {text("OP"), id(4), id(3), text("DiffuseColor")}},
    }).encode(out);
    out.append(13, '\0');
    return out;
}

} // namespace asset_fbx
} // namespace studio_test
