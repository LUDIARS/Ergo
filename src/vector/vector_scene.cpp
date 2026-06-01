#include "ergo/vector/vector.h"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace ergo::vector {
namespace {
float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }
Mat4 mat_identity() { Mat4 m{}; m.m[0]=m.m[5]=m.m[10]=m.m[15]=1.0f; return m; }
}

std::vector<VectorPath> morph_blend(const std::vector<VectorPath>& a, const std::vector<VectorPath>& b, float w);

std::unique_ptr<VectorScene> VectorScene::load_svg_file(const std::string& path) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs.good()) return {};
  std::string s((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  return load_svg_data(s);
}

std::unique_ptr<VectorScene> VectorScene::load_svg_data(std::string_view svg) {
  auto scene = std::make_unique<VectorScene>();
  auto parsed = parse_svg_data(svg);
  for (size_t i = 0; i < parsed.size(); ++i) {
    std::string id = parsed[i].id.empty() ? ("node_" + std::to_string(i)) : parsed[i].id;
    scene->add_path_node(id, {parsed[i]}, ExtrudeOptions{});
  }
  return scene;
}

void VectorScene::add_path_node(std::string id, std::vector<VectorPath> paths, ExtrudeOptions extrude) {
  Node n{}; n.id = id; n.base = paths; n.current = paths; n.extrude = extrude; n.mesh = build_mesh(paths, n.tess, n.extrude); n.dirty = false;
  nodes_[id] = std::move(n); dirty_ = true;
}
void VectorScene::set_node_transform(std::string_view id, const Transform& t) { auto it=nodes_.find(std::string(id)); if(it!=nodes_.end()){it->second.tf=t;dirty_=true;} }
void VectorScene::set_scale_x(std::string_view id, float s01) { auto it=nodes_.find(std::string(id)); if(it!=nodes_.end()){it->second.scale_x=s01;dirty_=true;} }
void VectorScene::set_color(std::string_view id, Rgba c) { auto it=nodes_.find(std::string(id)); if(it!=nodes_.end()){it->second.mat.color=c;dirty_=true;} }
void VectorScene::set_opacity(std::string_view id, float a01) { auto it=nodes_.find(std::string(id)); if(it!=nodes_.end()){it->second.mat.opacity=clamp01(a01);dirty_=true;} }
void VectorScene::set_extrude_depth(std::string_view id, float depth) { auto it=nodes_.find(std::string(id)); if(it!=nodes_.end()){it->second.extrude.depth=depth;it->second.dirty=true;dirty_=true;} }
void VectorScene::add_morph_target(std::string_view id, const std::vector<VectorPath>& state) { auto it=nodes_.find(std::string(id)); if(it!=nodes_.end()) it->second.morph_targets.push_back(state); }
void VectorScene::set_morph_weight(std::string_view id, float w01) { auto it=nodes_.find(std::string(id)); if(it!=nodes_.end()){it->second.morph_weight=clamp01(w01);it->second.dirty=true;dirty_=true;} }

void VectorScene::update(float /*dt*/) {
  for (auto& kv : nodes_) {
    auto& n = kv.second;
    if (n.morph_targets.empty()) continue;
    n.current = morph_blend(n.base, n.morph_targets.front(), n.morph_weight);
    n.mesh = build_mesh(n.current, n.tess, n.extrude);
    n.dirty = false;
  }
}

void VectorScene::collect(std::vector<DrawItem>& out) const {
  for (const auto& kv : nodes_) { DrawItem d{}; d.mesh = &kv.second.mesh; d.model = mat_identity(); d.model.m[0] = kv.second.scale_x; d.mat = kv.second.mat; out.push_back(d); }
}
bool VectorScene::dirty() const { return dirty_; }

} // namespace ergo::vector

