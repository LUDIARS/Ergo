#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ergo::ui_layout {

struct Rect { float x = 0; float y = 0; float w = 0; float h = 0; };
struct Anchor { std::string h = "left"; std::string v = "top"; };
struct Stretch { bool has_left = false; float left = 0; bool has_right = false; float right = 0;
                 bool has_top = false; float top = 0; bool has_bottom = false; float bottom = 0; };

struct TextureRef { std::string path; };

struct TextStyle {
    std::string font;
    float size = 16.0f;
    std::string align = "left";
    std::string color = "#ffffff";
};

struct VectorDrawItem {
    std::string source;
    Rect rect;
    float extrude = 0.0f;
    std::string fit = "stretch";
    std::string color = "#ffffff";
};

struct BindValue {
    enum class Kind : uint8_t { Number, Bool, String } kind = Kind::Number;
    double number = 0.0;
    bool boolean = false;
    std::string string;

    static BindValue from_number(double n) { BindValue v; v.kind = Kind::Number; v.number = n; return v; }
    static BindValue from_bool(bool b) { BindValue v; v.kind = Kind::Bool; v.boolean = b; return v; }
    static BindValue from_string(std::string s) { BindValue v; v.kind = Kind::String; v.string = std::move(s); return v; }
};

using BindContext = std::unordered_map<std::string, BindValue>;

struct BindRule {
    std::string target;
    std::string op;
    std::string expr;
};

struct Node {
    std::string id;
    std::string type = "container";
    std::string layout = "absolute";
    Rect rect;
    Rect base_rect;      // authored rect snapshot; scale_x derives from this to stay idempotent per-frame
    Rect resolved_rect;
    Anchor anchor;
    Stretch stretch;
    bool visible = true;
    float opacity = 1.0f;
    std::string color;
    std::string text_value;
    TextStyle text_style;
    TextureRef image;
    TextureRef nine_slice;
    VectorDrawItem vector;
    std::vector<BindRule> binds;
    std::vector<Node> children;
};

struct RenderAdapter {
    virtual ~RenderAdapter() = default;
    virtual void draw_rect(const Rect& rect, std::string_view color, float opacity) = 0;
    virtual void draw_nine_slice(const Rect& rect, const TextureRef& texture, float opacity) = 0;
    virtual void draw_image(const TextureRef& texture, const Rect& rect, float opacity) = 0;
    virtual void draw_text(std::string_view text, const TextStyle& style, const Rect& rect, float opacity) = 0;
    virtual void draw_vector_mesh(const VectorDrawItem& item, float opacity) = 0;
    virtual void push_clip(const Rect& rect) = 0;
    virtual void pop_clip() = 0;
};

class Document {
public:
    static std::unique_ptr<Document> load_file(const std::string& path);
    static std::unique_ptr<Document> load_json(std::string_view json);
    bool save_file(const std::string& path) const;

    void set_viewport(int w, int h);
    void update(const BindContext& ctx, float dt);
    void emit(RenderAdapter& adapter);

    Node* find(std::string_view id);
  // Current patch contract is id-based partial patch (not full RFC 7396 merge patch).
  void apply_patch(std::string_view json_merge_patch);
    std::string to_json() const;
    Node* hit_test(float x, float y);

private:
    int viewport_w_ = 1280;
    int viewport_h_ = 720;
    int design_w_ = 1280;
    int design_h_ = 720;
    int schema_version_ = 1;
    std::string name_;
    Node root_;
    std::string source_json_;

    void solve_layout_();
    void solve_node_(Node& node, const Rect& parent, bool preserve_position = false);
    void apply_binds_(Node& node, const BindContext& ctx);
    void emit_node_(Node& node, RenderAdapter& adapter);
    Node* find_node_(Node& node, std::string_view id);
    Node* hit_test_node_(Node& node, float x, float y);
};

} // namespace ergo::ui_layout
