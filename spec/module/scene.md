# Scene モジュール定義

## 概要

`ergo_scene` は **ルックデヴ用の自由操作シーン**を提供し、 同時に **将来の
GUI レベルエディタの土台**になるモジュール。

シーンは「カメラ + GameObject 群 + ポストプロセス設定」 から成る。 ホストは
シーンを読み込み、 フリーカメラで自由に見回し、 GameObject の transform /
マテリアルとポストプロセスを `tools/ergo` のエディタで調整できる。

設計の核心は **シーンを「コードで組む固定物」 でなく「データとして CRUD
できる編集対象ドキュメント」 として扱う**こと。 ルックデヴ用途では
「読み込んで・見て・調整する」 だけだが、 シーンモデルを最初から
addressable + シリアライズ可能に設計しておくことで、 後から GUI レベル
エディタ (オブジェクトの配置・追加・削除・保存) へ無改造で拡張できる。

## カテゴリ
システム

## 所属ドメイン
ルックデヴ / シーン構築 / レベルエディタ基盤 / 描画

## 依存
- `ergo_render` — 描画オーケストレーション層 (下記「描画」)。
  **現在 concurrent セッションが `feat/ergo-render` で実装中・main 未マージ**。
  `ergo_scene` の実装は `ergo_render` の main 取り込み後に行う。 本 spec は
  `ergo_render` の `RenderContext` / `FrameComposer` API を前提に設計する
- `ergo_actor` (+ `ergo_bind`) — GameObject のツリー登録と変数公開を流用
- C++17 標準ライブラリ (`<vector>`, `<string>`, `<memory>`, `<array>`)
- (テスト) GoogleTest 互換 mini-gtest

## 構成要素

### Scene
編集対象ドキュメントの単位。

- `camera` — フリーカメラ状態
- `objects` — `GameObject` の集合。 各要素は **文字列 id で addressable**
- `post_process` — ポストプロセス設定
- `load(path)` / `save(path)` — `*.scene.json` の読み書き

### GameObject
シーンに置かれる 1 オブジェクト。 `ergo::actor::Actor` を継承し、 シーン
ツリーへの登録と変数公開 (`bind_var`) を流用する。

| 属性 | 内容 |
|---|---|
| `id`        | シーン内一意の文字列 (シリアライズ・エディタ選択のキー) |
| `transform` | position / rotation(quat) / scale。 `ergo_actor` が次フェーズ送りにした transform を **本モジュールが持つ** |
| `visual`    | 描画リソース参照 (mesh / material 等)。 当面は `ResourceRef` (パス + kind) |
| `parent`    | 親 GameObject の id (`ergo_actor` のツリーに対応) |

`transform` 各成分と `visual` の主要パラメータを `bind_var` で公開する。

### Camera
ルックデヴ用フリーカメラ。 orbit (注視点まわり旋回) と fly (一人称移動) を
持つ。 位置 / 注視点 / FOV を `bind_var` 公開する。

### PostProcess
トーンマップ / bloom / カラーグレード等のパラメータ集合。 `ergo_render` の
`FrameComposer` のポストプロセスパス構成に対応づく。 各パラメータを
`bind_var` 公開する。

## 描画 — `ergo_render` との関係

`ergo_scene` は `ergo_render` の **上**に乗る。

- `ergo_render` が担うのは横断オーケストレーション (初期化順 / パス構成 /
  フレームループ / submit-present / screenshot)。
- `ergo_render` は「**GameObject → drawable 変換**」 と「**FrameComposer の
  パス構成 (ポストプロセス含む)**」 を *ゲーム側に残す* 設計。 `ergo_scene` が
  その「ゲーム側」 を引き受ける — シーンの GameObject を drawable に変換し、
  ポストプロセス設定をパス構成に反映する。
- 依存方向は `ergo_scene → ergo_render → Pictor` の一方向。

> 実装順: `ergo_render` が main にマージされてから `ergo_scene` の描画部を
> 結線する。 それまでは Scene / GameObject / Camera / シリアライズの
> データモデル層を先行実装できる (描画非依存)。

## エディタ調整 — `ergo_bind` 経由

GameObject の transform / マテリアル、 カメラ、 ポストプロセスのパラメータは
すべて `bind_var` で公開する。

- `tools/ergo` の `variable` プラグインでそのまま調整できる (既存機構)。
- 各 GameObject は `ergo_actor` のツリーノードなので、 variable プラグインの
  Actor ペインがそのまま **シーンツリービュー**になる。
- = ルックデヴ段階では「専用 UI を作らず」 既存 variable プラグインで
  パラメータ調整が成立する。

## レベルエディタ化の布石 (今後フェーズ)

本モジュールは「いずれ GUI レベルエディタになる」 ことを見越して設計する。

- **シーンはシリアライズ可能なドキュメント** (`*.scene.json`)。 GameObject の
  id / transform / visual / 親子、 カメラ、 ポストプロセスを全て保存・復元
  できる。 エディタの「保存」 はこの形式への書き出しになる。
- **GameObject は id で addressable**。 エディタが列挙・選択・追加・削除
  できる前提のコレクションにする (固定配列やハードコードにしない)。
- 将来 `tools/ergo` に専用 `scene` (レベルエディタ) プラグインを追加する:
  シーンツリー + 3D ビューポート + ギズモ + オブジェクトの配置 / 複製 /
  削除 + save/load。 `variable` プラグインの「値の調整」 から「オブジェクトの
  CRUD + 空間配置」 へ発展させる。
- そのため Scene / GameObject のデータモデルは初版から addressable +
  serializable で作る。 ルックデヴ初版とレベルエディタでモデルを作り直さない。

## シーンファイル形式 (`*.scene.json`)

```jsonc
{
  "version": 1,
  "camera": {
    "mode": "orbit",
    "target": [0, 0, 0],
    "distance": 8.0,
    "yaw": 0.6, "pitch": 0.3,
    "fov_deg": 50
  },
  "post_process": {
    "tonemap": "aces",
    "exposure": 1.0,
    "bloom": { "enabled": true, "threshold": 1.0, "intensity": 0.6 }
  },
  "objects": [
    {
      "id": "ground",
      "parent": null,
      "transform": { "pos": [0,0,0], "rot": [0,0,0,1], "scale": [10,1,10] },
      "visual": { "kind": "mesh", "ref": "assets/ground.mesh", "material": "assets/ground.mat" }
    }
  ]
}
```

JSON コーデックは `ergo_common` (`ergo::common::jsonm`) を流用する。

## 変数
- `Scene`: `camera`, `objects` (id → GameObject の所有コレクション),
  `post_process`
- `GameObject`: `id`, `transform`, `visual`, (Actor 由来) `handle` / `name` /
  `parent`
- `Camera`: `mode`, `target`, `distance`, `yaw`, `pitch`, `fov_deg`
  (fly モード時は `position` / `front`)

## 作業

### 入力
- `Scene::load(path)` — `*.scene.json` を読み、 GameObject を構築
- フリーカメラへの操作 (orbit ドラッグ / fly の WASD 相当) — ホストが
  入力を渡し `Camera::update(dt, input)` を呼ぶ
- `variable` プラグインからの `bind_var` 値変更 (transform / PP 等)

### 出力
- `Scene::save(path)` — 現在のシーンを `*.scene.json` に書き出す
- `Scene` の drawable 列 + パス構成を `ergo_render` に渡す (描画結線後)
- `bind_var` により公開された全パラメータ (variable プラグインへ)

### タスク
- シーン JSON のパース / 直列化 (`ergo_common::jsonm`)
- GameObject の生成時に `ergo_actor::Actor` として登録、 transform / visual を
  `bind_var` 公開
- フリーカメラの行列計算 (view / projection)
- GameObject → drawable 変換 (`ergo_render` 結線フェーズ)
- ダミープラグ (リンクのみ用の no-op)

## テスト
- 空シーンの生成 / GameObject 0 件
- `*.scene.json` を load → GameObject 数・id・transform が一致
- load した Scene を save → 再 load して round-trip 一致
- id で GameObject を引ける / 存在しない id は null
- GameObject 追加・削除でコレクションが更新される (エディタ前提の CRUD)
- Camera の orbit / fly で view 行列が妥当に変化する
- 親子 id 指定が `ergo_actor` のツリー親子に反映される

## 制約 / 先送り
- **描画結線は `ergo_render` の main 取り込み後**。 それまではデータモデル +
  シリアライズ + カメラ計算を先行実装する
- 初版 (ルックデヴ) は: シーン読み込み + フリーカメラ + `variable` プラグイン
  でのパラメータ調整まで。 GUI でのオブジェクト配置 / ギズモ / 新規作成は
  **レベルエディタフェーズ** (専用 `tools/ergo` プラグイン) に送る
- `ergo_actor` の reparent 未対応に倣い、 動的な親子付け替えは初版スコープ外
