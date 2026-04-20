#include "ergo/bind/types.h"

#include <algorithm>

namespace ergo::bind {

const char* to_string(VarKind k) {
    switch (k) {
        case VarKind::Bool:   return "bool";
        case VarKind::Int32:  return "int32";
        case VarKind::Int64:  return "int64";
        case VarKind::Float:  return "float";
        case VarKind::Double: return "double";
        case VarKind::String: return "string";
        case VarKind::Color:  return "color";
        case VarKind::Vec3:   return "vec3";
    }
    return "unknown";
}

VarKind kind_from_string(const std::string& s) {
    if (s == "bool")   return VarKind::Bool;
    if (s == "int32")  return VarKind::Int32;
    if (s == "int64")  return VarKind::Int64;
    if (s == "float")  return VarKind::Float;
    if (s == "double") return VarKind::Double;
    if (s == "string") return VarKind::String;
    if (s == "color")  return VarKind::Color;
    if (s == "vec3")   return VarKind::Vec3;
    return VarKind::Bool;
}

Value Value::of_bool(bool x)         { Value v; v.kind = VarKind::Bool;   v.b = x; return v; }
Value Value::of_int32(int32_t x)     { Value v; v.kind = VarKind::Int32;  v.i = x; return v; }
Value Value::of_int64(int64_t x)     { Value v; v.kind = VarKind::Int64;  v.i = x; return v; }
Value Value::of_float(float x)       { Value v; v.kind = VarKind::Float;  v.d = static_cast<double>(x); return v; }
Value Value::of_double(double x)     { Value v; v.kind = VarKind::Double; v.d = x; return v; }
Value Value::of_string(std::string x){ Value v; v.kind = VarKind::String; v.s = std::move(x); return v; }
Value Value::of_color(float r, float g, float b, float a) {
    Value v; v.kind = VarKind::Color;
    v.v[0] = r; v.v[1] = g; v.v[2] = b; v.v[3] = a;
    return v;
}
Value Value::of_vec3(float x, float y, float z) {
    Value v; v.kind = VarKind::Vec3;
    v.v[0] = x; v.v[1] = y; v.v[2] = z; v.v[3] = 0.0f;
    return v;
}

bool Value::equals(const Value& o) const {
    if (kind != o.kind) return false;
    switch (kind) {
        case VarKind::Bool:   return b == o.b;
        case VarKind::Int32:
        case VarKind::Int64:  return i == o.i;
        case VarKind::Float:
        case VarKind::Double: return d == o.d;
        case VarKind::String: return s == o.s;
        case VarKind::Color:  return v[0]==o.v[0] && v[1]==o.v[1] && v[2]==o.v[2] && v[3]==o.v[3];
        case VarKind::Vec3:   return v[0]==o.v[0] && v[1]==o.v[1] && v[2]==o.v[2];
    }
    return false;
}

Value clamp_to_meta(const Value& in, const VarMeta& meta) {
    if (meta.min >= meta.max) return in;
    Value out = in;
    auto clamp_d = [&](double x) { return std::min(meta.max, std::max(meta.min, x)); };
    switch (in.kind) {
        case VarKind::Int32:
        case VarKind::Int64:
            out.i = static_cast<int64_t>(clamp_d(static_cast<double>(in.i))); break;
        case VarKind::Float:
        case VarKind::Double:
            out.d = clamp_d(in.d); break;
        default: break;
    }
    return out;
}

} // namespace ergo::bind
