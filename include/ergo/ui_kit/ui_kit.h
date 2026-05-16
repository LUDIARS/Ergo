#pragma once

/// ergo_ui_kit — リテインドモードの UI コンポーネントフレームワーク。
///
/// 「UI コンポーネントの統括は Ergo、 描画は Pictor、 Input/アニメーションは
/// Ergo」 の役割分担 (spec/module/ui_framework.md)。 本モジュールはコンポー
/// ネントツリー・レイアウト解決・(P3+) 入力/アニメを担い、 出力は GPU 非依存の
/// 描画リスト (`DrawCmd` 配列)。 描画は Pictor `UIRenderer` が請け負う。
///
/// P1 のスコープ: コンポーネントツリー + auto-layout/constraints のレイアウト
/// 解決 + 描画リスト生成 + uidoc.json ロード。 入力ディスパッチとアニメは P3/P4。

#include "ergo/ui_kit/types.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ergo::ui_kit {

/// コンポーネントツリーの 1 ノード。
struct Node {
    std::string name;
    NodeKind    kind = NodeKind::Frame;

    // 宣言値 (レイアウト解決前)。
    Vec2  position;          // 親ローカル座標 (constraints モード時の基準)
    Vec2  size;              // 宣言サイズ (Sizing::Fixed 時に使う)
    float opacity = 1.0f;
    bool  visible = true;
    bool  clip    = false;   // Frame: 子を自分の矩形でクリップするか

    Style       style;
    LayoutSpec  layout;
    std::string text;        // Text ノードの文字列

    std::vector<Node> children;

    // レイアウト解決後 (UIContext::update が埋める)。
    Rect        resolved_rect;        // 画面 px
    float       resolved_opacity = 1.0f;
    std::string resolved_path;        // "Root/HUD/Btn" — 入力/アニメの参照キー
};

// ── 入力 (P3) ──────────────────────────────────────────────────────────
/// ポインタの生入力。 ホストが ergo_input 等から埋めて feed_input() に渡す。
struct PointerInput {
    float x = 0.0f, y = 0.0f;   // 画面 px (左上原点)
    bool  down = false;         // 主ボタン押下中
};

enum class EventKind : uint8_t {
    PointerEnter = 0, PointerExit = 1, PointerDown = 2, PointerUp = 3, Click = 4,
};

/// ディスパッチされる UI イベント。 `propagate` を false にするとバブリング停止。
struct Event {
    EventKind   kind = EventKind::Click;
    std::string node_path;      // イベントの起点ノードのパス
    float       x = 0.0f, y = 0.0f;
    bool        propagate = true;
};

// ── アニメーション (P4) ────────────────────────────────────────────────
enum class AnimProp : uint8_t {
    Opacity = 0, PosX = 1, PosY = 2, SizeW = 3, SizeH = 4,
    FillR = 5, FillG = 6, FillB = 7, FillA = 8,
};
enum class Easing : uint8_t { Linear = 0, EaseIn = 1, EaseOut = 2, EaseInOut = 3 };

/// Pictor UIRenderer が消費する描画コマンド (GPU 非依存)。
enum class DrawKind : uint8_t {
    Rect = 0, NineSlice = 1, Text = 2, Image = 3, PushClip = 4, PopClip = 5,
};

struct DrawCmd {
    DrawKind    kind = DrawKind::Rect;
    Rect        rect;
    Color       color;             // fill / tint / text color
    float       corner_radius = 0.0f;
    uint32_t    texture_id     = 0;
    float       nine[4]        = {0, 0, 0, 0};
    std::string text;
    float       font_size = 16.0f;
    TextAlign   text_align = TextAlign::Left;
    float       opacity   = 1.0f;
};

/// ロード済み UI ドキュメント。
struct Document {
    Node root;
};

/// uidoc.json (ergo_figma が出力 or 手書き) をロードする。
bool load_document(const std::string& uidoc_json_path, Document& out);
/// JSON 文字列から直接パースする。
bool parse_document(const std::string& json_text, Document& out);
/// Document を uidoc.json 文字列にシリアライズする。
std::string serialize_document(const Document& doc);

/// UI ランタイムコンテキスト。 ドキュメントを保持し、 毎フレーム
/// レイアウトを解決して描画リストを生成する。
class UIContext {
public:
    using EventHandler = std::function<void(Event&)>;

    void set_document(Document doc);
    void set_viewport(int width, int height);

    /// アニメ進行 → レイアウト解決 → 入力ディスパッチ → 描画リスト生成。
    void update(float dt);

    const std::vector<DrawCmd>& draw_list() const { return draw_list_; }
    Document&       document()       { return doc_; }
    const Document& document() const { return doc_; }

    /// "Root/HUD/HPBar" のようなスラッシュ区切りパスでノードを引く。
    Node* find(const std::string& path);

    // ── 入力 (P3) ──
    /// 最新のポインタ状態を渡す。 次の update() で hit-test / イベント発火する。
    void feed_input(const PointerInput& p);
    /// ノードパスにイベントハンドラを登録する。 イベントは起点 → 祖先へ
    /// バブリングする (Event::propagate=false で停止)。
    void on(const std::string& node_path, EventKind kind, EventHandler cb);

    // ── アニメーション (P4) ──
    /// ノードのプロパティを現在値から `to` へ `duration` 秒かけて補間する。
    /// 同じ (path, prop) の既存アニメは置き換える。
    void animate(const std::string& node_path, AnimProp prop, float to,
                 float duration, Easing easing = Easing::EaseOut);
    /// 指定ノード (とサブツリー) のアニメを全て止める。
    void stop_anim(const std::string& node_path);

private:
    void measure_(Node& n);
    void solve_layout_(Node& n, const Rect& frame, const std::string& parent_path);
    void emit_(const Node& n, float inherited_opacity);
    void advance_anims_(float dt);
    void dispatch_input_();
    void dispatch_event_(Event& ev);

    struct AnimTrack {
        std::string path;
        AnimProp    prop;
        float       from = 0.0f, to = 0.0f, dur = 1.0f, t = 0.0f;
        Easing      easing = Easing::EaseOut;
        bool        started = false;
    };
    struct HandlerEntry { EventKind kind; EventHandler cb; };

    Document doc_;
    int      vw_ = 1280;
    int      vh_ = 720;
    std::vector<DrawCmd> draw_list_;

    std::vector<AnimTrack> anims_;
    std::unordered_map<std::string, std::vector<HandlerEntry>> handlers_;

    PointerInput pointer_{};
    PointerInput prev_pointer_{};
    std::string  hovered_path_;
    std::string  pressed_path_;
    bool         first_input_ = true;
};

} // namespace ergo::ui_kit
