#include "model_analysis.h"

#include "pictor/animation/fbx_importer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

namespace ergo::studio::plugins::asset_import {
namespace {

std::string texture_slot(const std::string& channel) {
    if (channel == "DiffuseColor") return "diffuse";
    if (channel == "NormalMap" || channel == "Bump") return "normal";
    if (channel == "EmissiveColor") return "emissive";
    return channel;
}

} // namespace

ModelAnalysis analyze_fbx(const std::string& bytes) {
    pictor::FBXImporter loader;
    const auto* data = reinterpret_cast<const uint8_t*>(bytes.data());
    if (loader.detect_format(data, bytes.size()) == pictor::AnimationFormat::UNKNOWN) {
        throw std::runtime_error("unrecognized FBX format");
    }
    const auto result = loader.import_memory(data, bytes.size());
    if (!result.success || !result.scene) throw std::runtime_error(result.error_message);

    ModelAnalysis analysis;
    analysis.minimum.fill(std::numeric_limits<double>::infinity());
    analysis.maximum.fill(-std::numeric_limits<double>::infinity());
    for (auto id : result.resource_ids_of_type(pictor::FBXObjectType::GEOMETRY)) {
        const auto* obj = result.get_resource(id);
        if (!obj || !obj->geometry) continue;
        for (const auto& p : obj->geometry->positions) {
            const std::array<double, 3> point{p.x, p.y, p.z};
            for (std::size_t axis = 0; axis < point.size(); ++axis) {
                if (!std::isfinite(point[axis])) throw std::runtime_error("non-finite FBX vertex");
                analysis.minimum[axis] = std::min(analysis.minimum[axis], point[axis]);
                analysis.maximum[axis] = std::max(analysis.maximum[axis], point[axis]);
            }
        }
    }
    if (!std::isfinite(analysis.minimum[0])) throw std::runtime_error("FBX has no model geometry");

    std::map<std::string, pictor::VisusPart> parts;
    for (auto id : result.resource_ids_of_type(pictor::FBXObjectType::MATERIAL)) {
        const auto* obj = result.get_resource(id);
        if (!obj || !obj->material || obj->name.empty()) continue;
        auto& part = parts[obj->name];
        part.part = obj->name;
        part.metadata.set("material.shading_model", obj->material->shading_model);
        const std::map<std::string, pictor::FBXObjectId> textures(obj->material->textures.begin(),
                                                                obj->material->textures.end());
        for (const auto& [channel, texture_id] : textures) {
            const auto* texture = result.get_resource(texture_id);
            if (!texture || !texture->texture) continue;
            auto ref = texture->texture->relative_filename;
            if (ref.empty()) ref = texture->texture->file_name;
            if (!ref.empty()) {
                std::replace(ref.begin(), ref.end(), '\\', '/');
                part.metadata.set(std::string(pictor::visus_keys::kTexturePrefix) + texture_slot(channel), ref);
            }
        }
    }
    for (auto& entry : parts) analysis.parts.push_back(std::move(entry.second));
    for (const auto& clip : result.clips) analysis.clips.emplace_back(clip.name);
    // Unbound animation stacks may not project to runtime channels without a rig.
    for (auto id : result.resource_ids_of_type(pictor::FBXObjectType::ANIMATION_STACK)) {
        const auto* stack = result.get_resource(id);
        if (!stack || stack->name.empty()) continue;
        const auto existing = std::find_if(analysis.clips.begin(), analysis.clips.end(),
            [&](const auto& clip) { return clip.as_string() == stack->name; });
        if (existing == analysis.clips.end()) analysis.clips.emplace_back(stack->name);
    }
    std::sort(analysis.clips.begin(), analysis.clips.end(),
        [](const auto& a, const auto& b) { return a.as_string() < b.as_string(); });
    return analysis;
}

} // namespace ergo::studio::plugins::asset_import
