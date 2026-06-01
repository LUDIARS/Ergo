#include "ergo/vector/vector.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <regex>
#include <sstream>

namespace ergo::vector {
namespace {
std::string attr(const std::string& tag, const char* key) {
  std::regex re(std::string(key) + "=\"([^\"]*)\"");
  std::smatch m;
  if (std::regex_search(tag, m, re)) return m[1].str();
  return {};
}
float f(const std::string& s, float def = 0.0f) { return s.empty() ? def : std::stof(s); }
std::vector<float> nums(const std::string& s) {
  std::vector<float> out;
  std::regex re(R"([-+]?[0-9]*\.?[0-9]+)");
  for (auto it = std::sregex_iterator(s.begin(), s.end(), re); it != std::sregex_iterator(); ++it) out.push_back(std::stof((*it).str()));
  return out;
}
VectorPath from_polygon(const std::string& points, bool close) {
  VectorPath p;
  auto ns = nums(points);
  if (ns.size() < 2) return p;
  PathPoint m{}; m.command = PathCmd::MoveTo; m.p = {ns[0], ns[1]}; p.points.push_back(m);
  for (size_t i = 2; i + 1 < ns.size(); i += 2) { PathPoint l{}; l.command = PathCmd::LineTo; l.p = {ns[i], ns[i + 1]}; p.points.push_back(l); }
  if (close) { PathPoint z{}; z.command = PathCmd::Close; p.points.push_back(z); }
  return p;
}
VectorPath from_rect(const std::string& tag) {
  float x = f(attr(tag, "x")), y = f(attr(tag, "y")), w = f(attr(tag, "width")), h = f(attr(tag, "height"));
  return from_polygon(std::to_string(x) + "," + std::to_string(y) + " " + std::to_string(x + w) + "," + std::to_string(y) + " " +
                      std::to_string(x + w) + "," + std::to_string(y + h) + " " + std::to_string(x) + "," + std::to_string(y + h), true);
}
VectorPath from_path_d(const std::string& d) {
  VectorPath p;
  std::vector<std::string> tok; std::string cur;
  for (char c : d) { if (std::isalpha(static_cast<unsigned char>(c))) { if (!cur.empty()) { tok.push_back(cur); cur.clear(); } tok.push_back(std::string(1, c)); } else { cur.push_back(c); } }
  if (!cur.empty()) tok.push_back(cur);
  Vec2 pen{};
  for (size_t i = 0; i + 1 < tok.size(); i += 2) {
    char op = tok[i][0]; auto ns = nums(tok[i + 1]);
    if (op == 'M' || op == 'm') { if (ns.size() >= 2) { pen = {ns[0], ns[1]}; PathPoint a{}; a.command = PathCmd::MoveTo; a.p = pen; p.points.push_back(a);} }
    else if (op == 'L' || op == 'l') { for (size_t j = 0; j + 1 < ns.size(); j += 2) { pen = {ns[j], ns[j + 1]}; PathPoint a{}; a.command = PathCmd::LineTo; a.p = pen; p.points.push_back(a);} }
    else if (op == 'Q' || op == 'q') { for (size_t j = 0; j + 3 < ns.size(); j += 4) { PathPoint a{}; a.command = PathCmd::QuadTo; a.c1={ns[j],ns[j+1]}; a.p={ns[j+2],ns[j+3]}; p.points.push_back(a); } }
    else if (op == 'C' || op == 'c') { for (size_t j = 0; j + 5 < ns.size(); j += 6) { PathPoint a{}; a.command = PathCmd::CubicTo; a.c1={ns[j],ns[j+1]}; a.c2={ns[j+2],ns[j+3]}; a.p={ns[j+4],ns[j+5]}; p.points.push_back(a);} }
    else if (op == 'Z' || op == 'z') { PathPoint a{}; a.command = PathCmd::Close; p.points.push_back(a); }
  }
  return p;
}
}

std::vector<VectorPath> parse_svg_data(std::string_view svg) {
  std::vector<VectorPath> out;
  std::string s(svg);
  std::regex elem(R"(<[^>]+>)");
  for (auto it = std::sregex_iterator(s.begin(), s.end(), elem); it != std::sregex_iterator(); ++it) {
    std::string tag = (*it).str();
    std::string type;
    if (tag.find("<path") == 0) type = "path";
    else if (tag.find("<rect") == 0) type = "rect";
    else if (tag.find("<polygon") == 0) type = "polygon";
    else if (tag.find("<polyline") == 0) type = "polyline";
    else if (tag.find("<line") == 0) type = "line";
    else if (tag.find("<circle") == 0) type = "circle";
    else if (tag.find("<ellipse") == 0) type = "ellipse";
    else continue;
    VectorPath p;
    if (type == "path") p = from_path_d(attr(tag, "d"));
    else if (type == "rect") p = from_rect(tag);
    else if (type == "polygon") p = from_polygon(attr(tag, "points"), true);
    else if (type == "polyline") p = from_polygon(attr(tag, "points"), false);
    else if (type == "line") p = from_polygon(attr(tag, "x1")+","+attr(tag,"y1")+" "+attr(tag,"x2")+","+attr(tag,"y2"), false);
    else if (type == "circle") {
      float cx=f(attr(tag,"cx")), cy=f(attr(tag,"cy")), r=f(attr(tag,"r"));
      std::ostringstream ss; for (int i=0;i<16;++i){ float a=6.2831853f*i/16.0f; ss<<cx+std::cos(a)*r<<","<<cy+std::sin(a)*r<<" "; }
      p = from_polygon(ss.str(), true);
    } else if (type == "ellipse") {
      float cx=f(attr(tag,"cx")), cy=f(attr(tag,"cy")), rx=f(attr(tag,"rx")), ry=f(attr(tag,"ry"));
      std::ostringstream ss; for (int i=0;i<16;++i){ float a=6.2831853f*i/16.0f; ss<<cx+std::cos(a)*rx<<","<<cy+std::sin(a)*ry<<" "; }
      p = from_polygon(ss.str(), true);
    }
    p.id = attr(tag, "id");
    auto fr = attr(tag, "fill-rule");
    p.fill_rule = (fr == "evenodd") ? FillRule::EvenOdd : FillRule::NonZero;
    p.opacity = f(attr(tag, "opacity"), 1.0f);
    if (!p.points.empty()) out.push_back(std::move(p));
  }
  return out;
}

} // namespace ergo::vector
