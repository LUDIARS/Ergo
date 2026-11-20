#include "ergo/vector/vector.h"

#include <lorica/input_budget.hpp>

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

// Hand-written scanners instead of std::regex: libstdc++'s regex matcher
// recurses once per matched character, so a long tag or attribute in an
// untrusted SVG overflowed the stack (and std::stof threw on bad numbers).
// The accepted syntax is otherwise unchanged: tags are "<" + one or more
// non-">" + ">", numbers [-+]?[0-9]*\.?[0-9]+.

namespace ergo::vector {
namespace {

/// The value of attribute `key` (key="value"). The name must start the tag's
/// attribute list or follow whitespace, so "d" is not found inside "id" and
/// "x" not inside "rx" (the regex matched those).
std::string attr(const std::string& tag, const char* key) {
  const std::string needle = std::string(key) + "=\"";
  for (auto at = tag.find(needle); at != std::string::npos; at = tag.find(needle, at + 1)) {
    if (at != 0 && !std::isspace(static_cast<unsigned char>(tag[at - 1]))) continue;
    const auto begin = at + needle.size();
    const auto end = tag.find('"', begin);
    if (end == std::string::npos) return {};
    return tag.substr(begin, end - begin);
  }
  return {};
}

bool is_digit(char c) { return c >= '0' && c <= '9'; }

/// A finite float, or nullopt-like `ok=false` (out of range / no number).
float to_float(const char* begin, bool& ok) {
  char* end = nullptr;
  const float v = std::strtof(begin, &end);
  ok = end != begin && std::isfinite(v);
  return v;
}

/// Leading number of an attribute value ("10px" -> 10); `def` when absent or not finite.
float f(const std::string& s, float def = 0.0f) {
  if (s.empty()) return def;
  bool ok = false;
  const float v = to_float(s.c_str(), ok);
  return ok ? v : def;
}

/// Every number token [-+]?[0-9]*\.?[0-9]+ in `s`, left to right (tokens that
/// overflow a float are skipped).
std::vector<float> nums(const std::string& s) {
  std::vector<float> out;
  std::size_t i = 0;
  while (i < s.size()) {
    std::size_t j = i;
    if (s[j] == '-' || s[j] == '+') ++j;
    std::size_t k = j;
    while (k < s.size() && is_digit(s[k])) ++k;
    std::size_t end = k;
    if (k < s.size() && s[k] == '.' && k + 1 < s.size() && is_digit(s[k + 1])) {
      end = k + 1;
      while (end < s.size() && is_digit(s[end])) ++end;
    }
    if (end == j) { ++i; continue; }  // no digits here: try the next position
    bool ok = false;
    const float v = to_float(s.substr(i, end - i).c_str(), ok);
    if (ok) out.push_back(v);
    i = end;
  }
  return out;
}

/// Counts every path point against the document's budget.
struct Builder {
  lorica::input_budget& budget;
  bool over = false;
  bool push(VectorPath& p, const PathPoint& pt) {
    if (over || !budget.consume(0, 1, 0)) { over = true; return false; }
    p.points.push_back(pt);
    return true;
  }
};

VectorPath from_polygon(Builder& b, const std::string& points, bool close) {
  VectorPath p;
  auto ns = nums(points);
  if (ns.size() < 2) return p;
  PathPoint m{}; m.command = PathCmd::MoveTo; m.p = {ns[0], ns[1]};
  if (!b.push(p, m)) return p;
  for (size_t i = 2; i + 1 < ns.size(); i += 2) {
    PathPoint l{}; l.command = PathCmd::LineTo; l.p = {ns[i], ns[i + 1]};
    if (!b.push(p, l)) return p;
  }
  if (close) { PathPoint z{}; z.command = PathCmd::Close; b.push(p, z); }
  return p;
}

VectorPath from_rect(Builder& b, const std::string& tag) {
  float x = f(attr(tag, "x")), y = f(attr(tag, "y")), w = f(attr(tag, "width")), h = f(attr(tag, "height"));
  return from_polygon(b, std::to_string(x) + "," + std::to_string(y) + " " + std::to_string(x + w) + "," + std::to_string(y) + " " +
                         std::to_string(x + w) + "," + std::to_string(y + h) + " " + std::to_string(x) + "," + std::to_string(y + h), true);
}

VectorPath from_path_d(Builder& b, const std::string& d) {
  VectorPath p;
  std::vector<std::string> tok; std::string cur;
  for (char c : d) { if (std::isalpha(static_cast<unsigned char>(c))) { if (!cur.empty()) { tok.push_back(cur); cur.clear(); } tok.push_back(std::string(1, c)); } else { cur.push_back(c); } }
  if (!cur.empty()) tok.push_back(cur);
  Vec2 pen{};
  for (size_t i = 0; i + 1 < tok.size() && !b.over; i += 2) {
    char op = tok[i][0]; auto ns = nums(tok[i + 1]);
    if (op == 'M' || op == 'm') { if (ns.size() >= 2) { pen = {ns[0], ns[1]}; PathPoint a{}; a.command = PathCmd::MoveTo; a.p = pen; b.push(p, a);} }
    else if (op == 'L' || op == 'l') { for (size_t j = 0; j + 1 < ns.size(); j += 2) { pen = {ns[j], ns[j + 1]}; PathPoint a{}; a.command = PathCmd::LineTo; a.p = pen; if (!b.push(p, a)) break;} }
    else if (op == 'Q' || op == 'q') { for (size_t j = 0; j + 3 < ns.size(); j += 4) { PathPoint a{}; a.command = PathCmd::QuadTo; a.c1={ns[j],ns[j+1]}; a.p={ns[j+2],ns[j+3]}; if (!b.push(p, a)) break; } }
    else if (op == 'C' || op == 'c') { for (size_t j = 0; j + 5 < ns.size(); j += 6) { PathPoint a{}; a.command = PathCmd::CubicTo; a.c1={ns[j],ns[j+1]}; a.c2={ns[j+2],ns[j+3]}; a.p={ns[j+4],ns[j+5]}; if (!b.push(p, a)) break;} }
    else if (op == 'Z' || op == 'z') { PathPoint a{}; a.command = PathCmd::Close; b.push(p, a); }
  }
  return p;
}

std::string ring(float cx, float cy, float rx, float ry) {
  std::ostringstream ss;
  for (int i = 0; i < 16; ++i) { float a = 6.2831853f * i / 16.0f; ss << cx + std::cos(a) * rx << "," << cy + std::sin(a) * ry << " "; }
  return ss.str();
}

} // namespace

bool parse_svg_data(std::string_view svg, const SvgLimits& limits, std::vector<VectorPath>& out) {
  out.clear();
  if (svg.size() > limits.max_source_bytes) return false;
  lorica::input_budget budget({limits.max_source_bytes, limits.max_points, limits.max_shapes, 0});
  Builder b{budget};
  const std::string s(svg);
  std::size_t pos = 0;
  while (pos < s.size()) {
    // A tag: "<", at least one character that is not ">", then ">".
    const auto open = s.find('<', pos);
    if (open == std::string::npos || open + 1 >= s.size()) break;
    if (s[open + 1] == '>') { pos = open + 1; continue; }
    const auto close = s.find('>', open + 1);
    if (close == std::string::npos) break;
    const std::string tag = s.substr(open, close - open + 1);
    pos = close + 1;

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
    if (type == "path") p = from_path_d(b, attr(tag, "d"));
    else if (type == "rect") p = from_rect(b, tag);
    else if (type == "polygon") p = from_polygon(b, attr(tag, "points"), true);
    else if (type == "polyline") p = from_polygon(b, attr(tag, "points"), false);
    else if (type == "line") p = from_polygon(b, attr(tag, "x1")+","+attr(tag,"y1")+" "+attr(tag,"x2")+","+attr(tag,"y2"), false);
    else if (type == "circle") { const float r = f(attr(tag, "r")); p = from_polygon(b, ring(f(attr(tag, "cx")), f(attr(tag, "cy")), r, r), true); }
    else if (type == "ellipse") p = from_polygon(b, ring(f(attr(tag, "cx")), f(attr(tag, "cy")), f(attr(tag, "rx")), f(attr(tag, "ry"))), true);
    if (b.over) { out.clear(); return false; }
    p.id = attr(tag, "id");
    auto fr = attr(tag, "fill-rule");
    p.fill_rule = (fr == "evenodd") ? FillRule::EvenOdd : FillRule::NonZero;
    p.opacity = f(attr(tag, "opacity"), 1.0f);
    if (!p.points.empty()) {
      if (!budget.consume(0, 0, 1)) { out.clear(); return false; }
      out.push_back(std::move(p));
    }
  }
  return true;
}

std::vector<VectorPath> parse_svg_data(std::string_view svg) {
  std::vector<VectorPath> out;
  parse_svg_data(svg, SvgLimits{}, out);  // over the limits: empty (the document is refused)
  return out;
}

} // namespace ergo::vector
