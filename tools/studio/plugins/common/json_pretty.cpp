#include "json_pretty.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace ergo::studio::plugins {

std::string number_text(double v) {
    if (!std::isfinite(v)) return "null";   // as JSON.stringify
    if (v == 0) return "0";                 // also -0
    char buf[64];
    for (int prec = 1; prec <= 17; ++prec) {
        std::snprintf(buf, sizeof(buf), "%.*g", prec, v);
        if (std::strtod(buf, nullptr) == v) break;
    }
    std::string s = buf;
    // JS writes 1e21 as "1e+21" too; small exponents (1e-7) are rare in profiles.
    return s;
}

OrderedJson OrderedJson::from(const JsonValue& v) {
    OrderedJson o;
    if (v.is_array()) {
        o.kind_ = Kind::Array;
        for (const auto& e : *v.a) o.items_.push_back(from(e));
    } else if (v.is_object()) {
        o.kind_ = Kind::Object;
        for (const auto& [k, e] : *v.o) o.fields_.emplace_back(k, from(e));
    } else {
        o.scalar_ = v;
    }
    return o;
}

OrderedJson OrderedJson::object() {
    OrderedJson o;
    o.kind_ = Kind::Object;
    return o;
}

OrderedJson OrderedJson::array() {
    OrderedJson o;
    o.kind_ = Kind::Array;
    return o;
}

OrderedJson& OrderedJson::add(std::string key, OrderedJson v) {
    fields_.emplace_back(std::move(key), std::move(v));
    return *this;
}

OrderedJson& OrderedJson::push(OrderedJson v) {
    items_.push_back(std::move(v));
    return *this;
}

std::string OrderedJson::pretty() const {
    std::string out;
    write(out, 0);
    return out;
}

void OrderedJson::write(std::string& out, int indent) const {
    const std::string pad(static_cast<std::size_t>(indent + 1) * 2, ' ');
    const std::string end_pad(static_cast<std::size_t>(indent) * 2, ' ');
    switch (kind_) {
        case Kind::Scalar:
            out += scalar_.is_number() ? number_text(scalar_.n) : to_json(scalar_);
            return;
        case Kind::Array:
            if (items_.empty()) { out += "[]"; return; }
            out += "[\n";
            for (std::size_t i = 0; i < items_.size(); ++i) {
                out += pad;
                items_[i].write(out, indent + 1);
                out += i + 1 < items_.size() ? ",\n" : "\n";
            }
            out += end_pad + "]";
            return;
        case Kind::Object:
            if (fields_.empty()) { out += "{}"; return; }
            out += "{\n";
            for (std::size_t i = 0; i < fields_.size(); ++i) {
                out += pad + to_json(jstr(fields_[i].first)) + ": ";
                fields_[i].second.write(out, indent + 1);
                out += i + 1 < fields_.size() ? ",\n" : "\n";
            }
            out += end_pad + "}";
            return;
    }
}

} // namespace ergo::studio::plugins
