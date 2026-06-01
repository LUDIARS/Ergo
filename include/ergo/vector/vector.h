#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ergo::vector {

struct Vec2 { float x = 0.0f; float y = 0.0f; };
struct Vec3 { float x = 0.0f; float y = 0.0f; float z = 0.0f; };
struct Rgba { float r = 1.0f; float g = 1.0f; float b = 1.0f; float a = 1.0f; };
struct Mat4 { float m[16] = {0}; };

enum class FillRule : uint8_t { NonZero = 0, EvenOdd = 1 };
enum class PathCmd : uint8_t { MoveTo = 0, LineTo = 1, QuadTo = 2, CubicTo = 3, Close = 4 };

struct PathPoint {
  PathCmd command = PathCmd::MoveTo;
  Vec2 p{};
  Vec2 c1{};
  Vec2 c2{};
};

struct VectorPath {
  std::string id;
  std::vector<PathPoint> points;
  FillRule fill_rule = FillRule::NonZero;
  Rgba fill = {};
  float opacity = 1.0f;
};

struct Vertex {
  Vec3 pos{};
  Vec3 normal{};
  Vec2 uv{};
  Rgba color{};
};

struct VectorMesh {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
};

struct TessOptions { float flatten_tol = 0.25f; FillRule rule = FillRule::NonZero; };
struct ExtrudeOptions { float depth = 0.0f; bool front = true; bool back = true; bool walls = true; };
struct Transform { Vec3 translate{}; Vec3 rotate{}; Vec3 scale{1.0f, 1.0f, 1.0f}; };
struct MaterialParams { Rgba color{}; float opacity = 1.0f; };

VectorMesh build_mesh(const std::vector<VectorPath>& paths, const TessOptions& tess, const ExtrudeOptions& ext);
std::vector<VectorPath> parse_svg_data(std::string_view svg);

class VectorScene {
public:
  static std::unique_ptr<VectorScene> load_svg_file(const std::string& path);
  static std::unique_ptr<VectorScene> load_svg_data(std::string_view svg);

  void add_path_node(std::string id, std::vector<VectorPath> paths, ExtrudeOptions extrude);
  void set_node_transform(std::string_view id, const Transform& t);
  void set_scale_x(std::string_view id, float s01);
  void set_color(std::string_view id, Rgba c);
  void set_opacity(std::string_view id, float a01);
  void set_extrude_depth(std::string_view id, float depth);
  void add_morph_target(std::string_view id, const std::vector<VectorPath>& state);
  void set_morph_weight(std::string_view id, float w01);
  void update(float dt);

  struct DrawItem { const VectorMesh* mesh = nullptr; Mat4 model{}; MaterialParams mat{}; };
  void collect(std::vector<DrawItem>& out) const;
  bool dirty() const;

private:
  struct Node {
    std::string id;
    std::vector<VectorPath> base;
    std::vector<VectorPath> current;
    std::vector<std::vector<VectorPath>> morph_targets;
    float morph_weight = 0.0f;
    Transform tf{};
    float scale_x = 1.0f;
    ExtrudeOptions extrude{};
    TessOptions tess{};
    VectorMesh mesh{};
    MaterialParams mat{};
    bool dirty = true;
  };
  std::unordered_map<std::string, Node> nodes_;
  bool dirty_ = false;
};

} // namespace ergo::vector

