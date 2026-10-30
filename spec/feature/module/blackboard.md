# Blackboard モジュール定義

## 概要

ゲーム全体から参照される名前付き共有パラメータ (例: スコア / プレイヤー
HP / ステージ進行状況) を **シングルトン Engine** に登録し、変化を購読
できる仕組み。各値は `Property<T>` という ReactiveProperty 相当の型に
包まれ、購読者へ値変化を通知する。

参考実装: VGA-Team2026/Foundation の `Blackboard` 機能 (Unity, R3 依存)。
Ergo 版は R3 / Unity 依存を取り払った C++17 ポート。

## 用途

- グローバル State (score / time / health) を疎結合に配信
- UI / システム間通信を「名前で結ぶ」(直接参照を避けてビルド分離を保つ)。
  表示側は `find<T>(name)` / `subscribe<T>(name, cb)` で名前と型だけを知り、
  ゲームルール側の構造体を include しない
- カテゴリ単位での一括購読解除 (シーン破棄時に「stage」カテゴリの全
  購読を一発解除、など)

## カテゴリ
システム (ゲームプレイ + 開発者ツール)

## 所属ドメイン
ゲームプレイ / 状態管理

## 必要なデータ

- 名前 → 型消し Property* + std::type_index 登録テーブル。各登録は、名前で
  結んだ購読の cleanup record も持つ (登録が消えるときに先に閉じるため)
- カテゴリ → cleanup record リスト (`shared_ptr<bool>` で fired flag を
  共有し、RAII Subscription とカテゴリ release が二重実行しても安全)
- 名前 → 購読数カウンタ (デバッグ用)

## 依存

- C++17 標準ライブラリ
- (テスト) GoogleTest 互換 mini-gtest
- 外部依存なし (R3 / Unity に相当する反応型ライブラリは内蔵 Property<T>
  で代替)

## 設計上の決定

- **Property は呼び出し側オーナー**。Foundation と同じく Blackboard は
  Property を所有せず、ポインタで参照するだけ (登録時に渡す)。
  Property 自身の生存期間は呼び出し側が管理 (typically クラスメンバ)。
- **distinct 通知**: `Property<T>::set` は **`T` に `operator==` がある
  場合のみ** 同値で通知をスキップ。無ければ毎回通知 (Vec3 等カスタム型は
  ユーザーが必要なら自分で `operator==` を定義する)。
- **二重 cleanup 安全**: Subscription の RAII デストラクタとカテゴリ
  release が同じ subscription を消そうとしても idempotent (shared_ptr
  内 fired フラグでガード)。
- **名前で結ぶ購読は登録に紐づく**: `subscribe<T>(name, cb, category)` の
  購読は、自分のカテゴリに加えて `name` の登録にも cleanup を残す。
  `unregister(name)` / 同名の再登録 (上書き) / 登録側カテゴリの
  `release` / `release_all` は、登録を消す **前に** その購読を閉じる
  (Property がまだ生きているうちに unsubscribe する)。これにより HUD など
  別カテゴリの購読者は、producer が `release("stage")` して Property を
  破棄した後に自分の `Subscription` を捨てても安全。ポインタ版
  `subscribe<T>(name, &prop, cb)` は登録に紐づかない (Property を持つ側が
  寿命を管理する)。
- **購読より先に登録する**: 名前で購読する時点で未登録・型違いなら警告 +
  無効な Subscription を返す。後から登録しても自動では結ばない (遅延
  バインドはしない)。型は `std::type_index` の完全一致で照合する
  (`int` と `int64_t` は別型)。
- **マクロ**: Foundation の `[BlackboardAttribute]` (C# reflection) は
  C++ では不可能。代替として `BLACKBOARD_REGISTER(name, prop, category)`
  マクロを提供。
- **スレッド安全性なし**: メインスレッド専用。マルチスレッド要求が出たら
  Engine 内部で mutex を足す。
- リリースビルドでも常時有効 (gameplay 機能のため compile-strippable に
  しない)。

## 主要 API

### `Property<T>`

```cpp
template <typename T>
class Property {
public:
    Property() = default;
    explicit Property(T initial);

    const T& get() const;
    T&       mutable_ref();           // 直接書き換える場合に使用 (notify はしない)
    void     set(T value);            // operator== があれば distinct 通知

    using OnChange = std::function<void(const T&)>;
    using Token    = uint64_t;

    Token subscribe(OnChange cb);     // 即購読、Token を返す (低レベル API)
    void  unsubscribe(Token);
};
```

### `Engine`

```cpp
class Engine {
    static Engine& instance();

    template <typename T>
    void register_property(std::string name, Property<T>* property,
                           std::string category = "");

    void unregister(const std::string& name);

    // 名前で引く (型が違えば警告 + nullptr、未登録は nullptr)
    template <typename T>
    Property<T>* find(const std::string& name) const;

    // 名前だけで購読 (未登録・型違い・null cb は警告 + 無効な Subscription)。
    // 登録が消えるときに自動で閉じる。
    template <typename T>
    [[nodiscard]] Subscription subscribe(
        const std::string& name,
        std::function<void(const T&)> on_change,
        std::string        category = "");

    // Property を持っている側の購読 (name はデバッグ用ラベル)
    template <typename T>
    [[nodiscard]] Subscription subscribe(
        const std::string& name,
        Property<T>*       property,
        std::function<void(const T&)> on_change,
        std::string        category = "");

    // 全登録を (名前, 型, 型消しポインタ) で列挙。順序は不定。
    // ホスト側アダプタ用。fn の中で登録 / 解除 / release しないこと。
    void visit(const std::function<void(const std::string&, std::type_index,
                                        const void*)>& fn) const;

    void release(const std::string& category);
    void release_all();

    // Debug
    std::vector<std::string> registered_property_names() const;
    std::size_t              subscription_count(const std::string& name) const;
    std::string              debug_info() const;
};
```

### `Subscription` (RAII)

```cpp
class Subscription {
    Subscription() = default;
    ~Subscription();                  // cleanup を 1 度だけ実行
    Subscription(Subscription&&);
    Subscription& operator=(Subscription&&);
    void reset();                     // 早期解除
    bool active() const;
};
```

### マクロ

```cpp
// register_property の薄いラッパ。category は省略可。
BLACKBOARD_REGISTER(name, lvalue [, category]);
```

## 利用例

```cpp
struct GameState {
    ergo::blackboard::Property<int>   score{0};
    ergo::blackboard::Property<float> health{100.0f};
};

GameState state;
auto& bb = ergo::blackboard::Engine::instance();

// 登録
BLACKBOARD_REGISTER("score",  state.score,  "stage");
BLACKBOARD_REGISTER("health", state.health, "stage");

// 購読 (表示側は GameState を知らず、名前と型だけで結ぶ)
auto sub = bb.subscribe<int>("score",
    [](const int& new_score) { hud.set_score(new_score); },
    "hud");

// 初期値は find で引く
if (const auto* hp = bb.find<float>("health")) hud.set_health(hp->get());

// Property を持っている側はポインタ版でもよい (登録には紐づかない)
auto own = bb.subscribe<int>("score", &state.score,
    [](const int& v) { log_score(v); },
    "stage");

// 値を更新
state.score.set(100);                  // → コールバック発火

// シーン終了: stage カテゴリの全購読 + 登録を解除。
// 登録が消えるので "hud" カテゴリの名前購読 (sub) もここで閉じる。
bb.release("stage");
```

### ホスト側アダプタ: ergo_ui_layout の BindContext に流す

blackboard と ui_layout はお互いを include しない (モジュール間 import 禁止)。
HUD を `ergo::ui_layout::Document` で描くホストは、次のような小さな
アダプタを **ホスト側** に置く (どちらのモジュールにも入れない)。

```cpp
#include "ergo/blackboard/blackboard.h"
#include "ergo/ui_layout/ui_layout.h"

ergo::ui_layout::BindContext blackboard_to_bind_context() {
    namespace bb = ergo::blackboard;
    using ergo::ui_layout::BindValue;

    ergo::ui_layout::BindContext ctx;
    bb::Engine::instance().visit(
        [&ctx](const std::string& name, std::type_index type, const void* p) {
            if (type == std::type_index(typeid(std::int32_t))) {
                ctx[name] = BindValue::from_number(
                    static_cast<const bb::Property<std::int32_t>*>(p)->get());
            } else if (type == std::type_index(typeid(std::int64_t))) {
                ctx[name] = BindValue::from_number(static_cast<double>(
                    static_cast<const bb::Property<std::int64_t>*>(p)->get()));
            } else if (type == std::type_index(typeid(float))) {
                ctx[name] = BindValue::from_number(
                    static_cast<const bb::Property<float>*>(p)->get());
            } else if (type == std::type_index(typeid(bool))) {
                ctx[name] = BindValue::from_bool(
                    static_cast<const bb::Property<bool>*>(p)->get());
            } else if (type == std::type_index(typeid(std::string))) {
                ctx[name] = BindValue::from_string(
                    static_cast<const bb::Property<std::string>*>(p)->get());
            }   // それ以外の型は HUD に出さない
        });
    return ctx;
}

// 毎フレーム (ゲームルールの更新の後):
document.update(blackboard_to_bind_context(), real_dt);
```

使う名前と型が決まっているなら `find<T>` で必要な分だけ詰めればよい
([`spec/setup/integrate-modules.md`](../../setup/integrate-modules.md)
「HUD への値の受け渡し」)。フレームループ全体の参照実装は
`examples/compose_loop/main.cpp` (producer が "stage"、HUD が "hud" カテゴリ)。

## 作業

### 入力

- `register_property(name, ptr, category)` — 既存 Property を登録
- `unregister(name)` — 名前指定で登録解除
- `subscribe<T>(name, ptr, cb, category)` — 購読、Subscription RAII を返す
- `subscribe<T>(name, cb, category)` — 名前だけで購読 (登録に紐づく)
- `find<T>(name)` — 型を照合して登録済み Property を引く
- `visit(fn)` — 全登録を (名前, 型, ポインタ) で列挙
- `release(category)` — カテゴリ単位で全購読 + 登録を解除
- `release_all()` — 全状態を初期化
- `Property<T>::set(value)` — 値更新、distinct なら購読者に通知

### 出力

- 各購読者の `on_change(value)` 呼び出し
- デバッグ用クエリ: `registered_property_names()`、`subscription_count(name)`、
  `debug_info()` (人間可読サマリ)

### タスク

- 重複登録は **既存を上書き + 警告ログ** (Foundation 互換)
- カテゴリ "" は内部で "Default" として扱う (Foundation 互換)
- Subscription の destructor またはカテゴリ release のどちらが先でも
  cleanup は **1 度だけ** 実行 (shared_ptr<bool> fired flag)
- nullptr を渡された場合は no-op + 警告
- 名前購読: 未登録・型違い・null コールバックは警告 + 無効な Subscription
- `unregister` / 再登録 / 登録側カテゴリの `release` / `release_all` は、
  その登録に紐づく名前購読を先に閉じてから登録を消す
- `release_all` は internal state を完全初期化 (テスト間の独立性確保用)

### プラットフォーム別タスク

- 全 OS 同等。プラットフォーム依存なし。

# テスト

- 新規 Property の登録 → enumerate に含まれること
- subscribe → set で onChange が呼ばれる
- 同じ値を set しても (operator== があれば) 通知されない
- operator== が無い型は毎回通知される
- subscribe された subscription を destruct すると以降通知されない
- カテゴリ単位で release すると全 subscription が一斉に解除される
- subscription destruct + release が両方発火しても二重 cleanup しない
- 重複名 register は警告 + 上書き
- nullptr property の register / subscribe は no-op
- release("") は "Default" カテゴリを解除
- release_all で全ての登録 / 購読がリセットされる
- subscription_count(name) が増減を正しく反映
- debug_info() に登録名とカテゴリ名が含まれる
- `find<T>` が登録済みの Property を返し、型違い・未登録では nullptr
- 名前購読が `set` で発火し、reset 後は発火しない
- 名前購読の型違い・未登録・null コールバックは無効 (購読数 0、後から
  登録しても結ばれない)
- `subscribe<T>(name, nullptr, cb)` は従来のポインタ版に解決される
- `unregister` / 登録側カテゴリの `release` で、別カテゴリにある名前購読が
  閉じる。以降 `set` しても発火せず、Property 破棄後に Subscription を
  捨てても安全 (ASan で検証)
- 同名の再登録で古い名前購読が閉じ、新しい登録に購読し直せる
- 購読側カテゴリの `release` は登録を残す / `release_all` は名前購読も閉じる
- `visit` が全登録の名前・型・ポインタを列挙する
- ポインタ購読と名前購読が混在しても `subscription_count` が整合する
