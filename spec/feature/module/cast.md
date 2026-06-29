# Cast モジュール定義

## 概要

`ergo_cast` は既存の `ergo::actor::Actor` 群を **名前付き「Scene」に束ねて
一括でライフサイクル管理する軽量モジュール**。 ホストは複数の Actor を 1 つの
Scene にまとめ、 まとめて on/off したり、 現在のアクティブ状態を snapshot
として取り出し・復元したりできる。

設計の核心は **「論理的なグルーピング + activation + snapshot」 だけに責務を
絞る**こと。 Actor を **所有しない** (raw `Actor*` を保持するだけで delete
しない。 寿命はホストが持つ)。 メンバ判定は Actor が持つ安定な
`handle()` で行う。 I/O も JSON も描画も行わない — `SceneSnapshot` は素の
データ構造で、 直列化が必要ならコンシューマ側が好きな形式で行う。

**`ergo_scene` とは別物**。 `ergo_scene` は GUI 編集可能な look-dev
**ドキュメント** (カメラ + GameObject 群 + ポストプロセスを `*.scene.json` に
直列化し、 `ergo_render` の描画オーケストレーションに乗る重いモジュール)。
対して `ergo_cast` は **描画非依存** (Pictor も `ergo_render` も include
しない) の薄い実行時キャストリストで、 描画とは一切結合しない。 両者は
用途が異なるため共存する:

| | `ergo_cast` | `ergo_scene` |
|---|---|---|
| 役割 | Actor の軽量な束ね + 一括 activate/deactivate + snapshot | GUI 編集可能な look-dev ドキュメント |
| 所有 | 非所有 (raw `Actor*`) | GameObject を所有・CRUD |
| 描画 | **非依存** (rendering include 一切なし) | `ergo_render` → Pictor に依存 |
| 直列化 | 素の `SceneSnapshot` 構造体のみ (codec なし) | `*.scene.json` (`ergo_common::jsonm`) |

## カテゴリ

システム

## 所属ドメイン

シーン / Actor グルーピング / ライフサイクル管理 (描画非依存)

## 必要なデータ

- ホストが生成・所有する `ergo::actor::Actor` インスタンス群 (非所有参照)。
- Scene 名 (`std::string`)。
- (任意) アクティブ遷移時に呼ばれるコールバック群 (`std::function<void(Actor*)>`)。

## 依存

- C++17 標準ライブラリ: `<string>`, `<vector>`, `<functional>`, `<cstddef>`,
  `<algorithm>`
- `ergo_actor` (推移的に `ergo_bind`) — Actor の安定 handle / 名前を流用する。
  Actor は基底型 (foundational base type) なので、 これに乗ることは Ergo の
  「`ergo_<domain>` 同士の cross-import 禁止」 に抵触しない (`ergo_scene` も
  同じく Actor の上に乗る)。
- (テスト) GoogleTest 互換 mini-gtest

> **描画への依存はゼロ**。 Pictor / `ergo_render` / その他レンダリングヘッダを
> 一切 include しない。

## 変数

実装内部の主要な状態:

- `name_` — Scene 名
- `scene_active_` — Scene 全体の activation フラグ
- `entries_` — メンバの `std::vector<Entry>` (`Entry { Actor* actor; bool active; }`)。
  挿入順を保持し、 メンバ判定は `actor->handle()` の一致で行う
- `on_activate_` / `on_deactivate_` — 蓄積されるコールバック列

メンバの **実効 (effective) アクティブ状態** は常に
`scene_active_ && entry.active`。 コールバックはこの実効状態が遷移したときだけ
発火する。

## 作業

### 入力 (ホストから呼ばれる API)

- **メンバ操作**
  - `add(Actor* a, bool active = true)` — null または handle 重複なら no-op
  - `remove(Actor* a)` / `remove(Handle h)` — 削除できれば true
  - `contains(const Actor* a)` / `contains(Handle h)`
  - `size()` / `actors()` (挿入順のポインタ列)
- **Scene 単位の activation**
  - `activate()` / `deactivate()` / `active()`
- **Actor 単位の activation**
  - `activate(Actor* a)` / `deactivate(Actor* a)` — 非メンバなら no-op
  - `is_active(const Actor* a)` — 実効状態 (非メンバは false)
- **コールバック登録**
  - `on_activate(cb)` / `on_deactivate(cb)` — 複数登録は蓄積され全て呼ばれる
- **snapshot / restore**
  - `snapshot()` → `SceneSnapshot`
  - `restore(const SceneSnapshot&)`

### 出力

- `actors()` / `snapshot()` の戻り値。
- 実効状態が遷移したメンバについて発火する `on_activate` / `on_deactivate`
  コールバック (副作用はコールバック実装側が決める。 本モジュールは描画もログも
  行わない)。

### タスク (状態遷移ルール)

- `activate()`: Scene が off→on のときのみ、 per-actor フラグが立っている各
  メンバに `on_activate` を発火 (実効 false→true)。 既に on なら no-op。
- `deactivate()`: Scene が on→off のときのみ、 現在実効アクティブな各メンバに
  `on_deactivate` を発火 (実効 true→false)。 既に off なら no-op。
- `activate(Actor*)` / `deactivate(Actor*)`: per-actor フラグを更新し、 実効
  状態 (`scene_active_ && entry.active`) が変化したときだけ対応コールバックを
  発火。 フラグが既に同値なら no-op。
- `restore()`: `scene_active_` を復元し、 handle が現メンバと一致する entry の
  per-actor フラグを復元する。 **未知の handle は無視** (Actor が既に外されて
  いる可能性があるため)。 メンバの追加・削除は行わない。 フラグ適用後、 実効
  状態が変化したメンバについてコールバックを発火する。

## API データ構造

```cpp
namespace ergo::cast {

using Actor  = ergo::actor::Actor;
using Handle = ergo::actor::Handle;   // = uint64_t

struct ActorEntry {
    Handle      handle;
    std::string name;
    bool        active;     // per-actor フラグ (実効状態ではない)
};

struct SceneSnapshot {
    std::string             scene_name;
    bool                    scene_active = false;
    std::vector<ActorEntry> entries;
};

class Scene { /* 上記 API */ };

} // namespace ergo::cast
```

`SceneSnapshot` は **素のデータ構造**。 永続化が要るならコンシューマが
`ergo_common::jsonm` 等で直列化する (本モジュールには codec を入れない =
単一責任)。

## テスト

- add / contains / size / remove (ポインタ・handle 両方)、 null 追加は no-op、
  同一 handle の重複追加は no-op。
- `actors()` が挿入順を返す。
- Scene の activate/deactivate が `active()` と各メンバの `is_active()` を
  トグルする。
- Actor 単位の activate/deactivate、 `is_active` が `scene_active && entry.active`
  を反映する。
- コールバックが実効状態の遷移時だけ発火する (発火回数を数え、 既に同状態の
  Actor を deactivate しても発火しないことを確認)。
- snapshot が状態を捕捉する。 restore が per-actor フラグ + `scene_active` を
  再適用し、 未知 handle を無視し、 メンバ構成を変えず、 変化した entry に
  ついてコールバックを発火する。