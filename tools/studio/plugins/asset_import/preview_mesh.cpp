#include "preview_mesh.h"
#include "model_analysis.h"

#include "pictor/animation/fbx_importer.h"
#include "pictor/core/transform_math.h"

#include <lorica/input_budget.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace ergo::studio::plugins::asset_import {
namespace {
using pictor::float3;

double dot(const float3& a, const float3& b) {
    return static_cast<double>(a.x) * b.x + static_cast<double>(a.y) * b.y +
           static_cast<double>(a.z) * b.z;
}

float3 cross(const float3& a, const float3& b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}

float3 camera_space(const float3& p) {
    return {static_cast<float>(dot(p, {.707107f, 0, -.707107f})),
            static_cast<float>(dot(p, {.301511f, -.904534f, .301511f})),
            static_cast<float>(dot(p, {.639602f, .426401f, .639602f}))};
}

struct Triangle {
    std::array<float3, 3> points;
    float3 color;
};

std::vector<Triangle> triangles(const pictor::FBXScene& scene) {
    constexpr std::size_t max_triangles = 500000;
    lorica::input_budget budget({max_triangles * sizeof(Triangle), max_triangles, max_triangles, 1});
    std::vector<Triangle> result;
    for (const auto id : scene.ids_of_type(pictor::FBXObjectType::GEOMETRY)) {
        const auto* mesh = scene.triangulate(id);
        if (!mesh) continue;
        auto transform = pictor::float4x4::identity();
        std::vector<const pictor::FBXMaterial*> materials;
        for (const auto parent : scene.parents_of(id)) {
            const auto* model = scene.get(parent);
            if (!model || !model->model) continue;
            transform = scene.evaluate_world_transform(parent);
            for (const auto child : scene.children_of(parent)) {
                const auto* material = scene.get(child);
                if (material && material->material) materials.push_back(material->material.get());
            }
            break;
        }
        for (std::size_t i = 0; i < mesh->indices.size() / 3; ++i) {
            if (!budget.consume(sizeof(Triangle), 1, 1)) throw std::runtime_error("preview triangle limit exceeded");
            Triangle tri{};
            for (std::size_t k = 0; k < 3; ++k) {
                const auto index = mesh->indices[i*3 + k];
                if (index < 0 || static_cast<std::size_t>(index) >= mesh->positions.size()) {
                    throw std::runtime_error("invalid FBX mesh index");
                }
                const auto p = pictor::transform_math::transform_point(transform,
                    mesh->positions[static_cast<std::size_t>(index)]);
                auto v = camera_space({p.x, p.y, p.z});
                if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) {
                    throw std::runtime_error("non-finite FBX preview vertex");
                }
                tri.points[k] = v;
            }
            const auto normal = cross(tri.points[1] - tri.points[0], tri.points[2] - tri.points[0]);
            const auto len = std::sqrt(dot(normal, normal));
            const auto diffuse = len > 0 && std::isfinite(len)
                ? std::abs(dot(normal, {-.4f, -.4f, .8246f})) / len : 0.0;
            auto color = float3{.68f, .74f, .85f};
            if (i < mesh->material_per_tri.size()) {
                const auto slot = static_cast<std::size_t>(mesh->material_per_tri[i]);
                if (slot < materials.size()) color = materials[slot]->diffuse;
            }
            tri.color = color * static_cast<float>(.25 + .75 * diffuse);
            result.push_back(tri);
        }
    }
    return result;
}

} // namespace

std::vector<PreviewVertex> preview_mesh(SourceFormat format, const std::string& bytes) {
    if (const auto* issue = model_loader_issue(format)) throw std::runtime_error(issue);
    const pictor::FBXImporter loader;
    const auto model = loader.import_memory(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    if (!model.success || !model.scene) throw std::runtime_error(model.error_message);
    auto mesh = triangles(*model.scene);
    if (mesh.empty()) throw std::runtime_error("FBX has no drawable triangles");
    auto low = mesh.front().points[0];
    auto high = low;
    for (const auto& triangle : mesh) for (const auto& p : triangle.points) {
        low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
        high = {std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z)};
    }
    const double width = static_cast<double>(high.x) - low.x;
    const double height = static_cast<double>(high.y) - low.y;
    const double depth = static_cast<double>(high.z) - low.z;
    const double side = std::max({width, height, 1e-9});
    const double scale = 1.65 / side;
    std::vector<PreviewVertex> vertices;
    vertices.reserve(mesh.size() * 3);
    for (const auto& triangle : mesh) for (const auto& p : triangle.points) {
        const float3 position{
            static_cast<float>((p.x - (low.x + width / 2)) * scale),
            static_cast<float>((p.y - (low.y + height / 2)) * scale),
            static_cast<float>(depth > 0 ? (high.z - static_cast<double>(p.z)) / depth : .5)};
        vertices.push_back({position, triangle.color});
    }
    return vertices;
}

} // namespace ergo::studio::plugins::asset_import
