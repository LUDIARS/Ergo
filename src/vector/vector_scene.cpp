#include "ergo/vector/vector.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>

namespace ergo::vector {
namespace {
float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }
Mat4 mat_identity() { Mat4 m{}; m.m[0]=m.m[5]=m.m[10]=m.m[15]=1.0f; return m; }
Mat4 mat_mul(const Mat4& a, const Mat4& b) {
  Mat4 o{};
  for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) {
    o.m[r*4+c] = 0.0f;
    for (int k = 0; k < 4; ++k) o.m[r*4+c] += a.m[r*4+k] * b.m[k*4+c];
  }
  return o;
}
Mat4 mat_translate(const Vec3& t) { Mat4 m = mat_identity(); m.m[3]=t.x; m.m[7]=t.y; m.m[11]=t.z; return m; }
Mat4 mat_scale(const Vec3& s) { Mat4 m = mat_identity(); m.m[0]=s.x; m.m[5]=s.y; m.m[10]=s.z; return m; }
Mat4 mat_rot_x(float a){ Mat4 m=mat_identity(); float c=std::cos(a), s=std::sin(a); m.m[5]=c; m.m[6]=-s; m.m[9]=s; m.m[10]=c; return m; }
Mat4 mat_rot_y(float a){ Mat4 m=mat_identity(); float c=std::cos(a), s=std::sin(a); m.m[0]=c; m.m[2]=s; m.m[8]=-s; m.m[10]=c; return m; }
Mat4 mat_rot_z(float a){ Mat4 m=mat_identity(); float c=std::cos(a), s=std::sin(a); m.m[0]=c; m.m[1]=-s; m.m[4]=s; m.m[5]=c; return m; }
Mat4 mat_trs(const Transform& t, float sx) {
  Transform tt = t;
  tt.scale.x *= sx;
  return mat_mul(mat_translate(tt.translate),
         mat_mul(mat_mul(mat_rot_z(tt.rotate.z), mat_mul(mat_rot_y(tt.rotate.y), mat_rot_x(tt.rotate.x))),
                 mat_scale(tt.scale)));
}
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
  for (const auto& kv : nodes_) {
    DrawItem d{};
    d.mesh = &kv.second.mesh;
    d.model = mat_trs(kv.second.tf, kv.second.scale_x);
    d.mat = kv.second.mat;
    out.push_back(d);
  }
}
bool VectorScene::dirty() const { return dirty_; }

} // namespace ergo::vector
