#pragma once
/// Key-ordered, indented JSON output matching JavaScript's
/// JSON.stringify(value, null, 2) — for files people also edit by hand
/// (Pictor *.profile.json), where key order and layout must stay stable.
/// jsonm objects are std::map (sorted keys) and print compactly, so files
/// written through them would reorder and reflow on every save.
#include "json_util.h"

#include <string>
#include <utility>
#include <vector>

namespace ergo::studio::plugins {

class OrderedJson {
public:
    /// Scalar, or an array / object converted as is (objects in key order).
    static OrderedJson from(const JsonValue& v);
    static OrderedJson object();
    static OrderedJson array();

    OrderedJson& add(std::string key, OrderedJson v);   ///< object: keeps insertion order
    OrderedJson& push(OrderedJson v);                   ///< array

    /// JSON.stringify(v, null, 2) layout; no trailing newline.
    std::string pretty() const;

private:
    enum class Kind { Scalar, Array, Object };
    void write(std::string& out, int indent) const;

    Kind                                             kind_ = Kind::Scalar;
    JsonValue                                        scalar_;
    std::vector<OrderedJson>                         items_;
    std::vector<std::pair<std::string, OrderedJson>> fields_;
};

/// Shortest "%.Ng" form that reads back to the same double (JS number text
/// for the values profiles use: 1 -> "1", 0.025 -> "0.025").
std::string number_text(double v);

} // namespace ergo::studio::plugins
