#pragma once

/// Common types for ergo_bind. Mirrors variable-editor's wire schema (v1).

#include <cstdint>
#include <string>

namespace ergo::bind {

enum class VarKind : uint8_t {
    Bool   = 0,
    Int32  = 1,
    Int64  = 2,
    Float  = 3,
    Double = 4,
    String = 5,
    Color  = 6,  // RGBA in v[0..3]
    Vec3   = 7,  // XYZ in v[0..2]
};

const char* to_string(VarKind k);
VarKind     kind_from_string(const std::string& s);

struct VarMeta {
    double      min       = 0.0;
    double      max       = 0.0;   // min == max means "no range hint"
    double      step      = 0.0;
    bool        read_only = false;
    std::string category;
    std::string unit;
};

struct Value {
    VarKind     kind = VarKind::Bool;
    bool        b    = false;
    int64_t     i    = 0;
    double      d    = 0.0;
    float       v[4] = {0, 0, 0, 0};
    std::string s;

    static Value of_bool   (bool x);
    static Value of_int32  (int32_t x);
    static Value of_int64  (int64_t x);
    static Value of_float  (float x);
    static Value of_double (double x);
    static Value of_string (std::string x);
    static Value of_color  (float r, float g, float b, float a = 1.0f);
    static Value of_vec3   (float x, float y, float z);

    bool equals(const Value& other) const;
};

Value clamp_to_meta(const Value& v, const VarMeta& meta);

} // namespace ergo::bind
