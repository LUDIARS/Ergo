# Scene モジュール定義

## 概要

`ergo_scene` は **ルックデヴ用シーンを「データとして CRUD できる編集対象
ドキュメント」 として持つ**モジュール。 シーンは「カメラ + アクター群 +
ポストプロセス設定」 から成り、 `*.scene.json` として読み書きできる。

本モジュールが持つのは **データモデル層だけ**: ドキュメントの CRUD、 JSON
load/save、 フィールド単位のパッチ適用、 カメラ行列、 複数シーンの合成
(`SceneGraph`)。 描画 (アクター → drawable 変換、 ポストプロセスのパス構成)
と `ergo_actor` / `ergo_bind` への結線は **ホスト側の責務で、 現時点では
未実装 (将来)**。 `ergo_scene` はそれらに依存しない。

ライブ編集は `tools/ergo` の `scene_editor` プラグイン
([`spec/feature/tool/scene_editor.md`](../tool/scene_editor.md)) で行う。
エンジン側のクライアントは別ターゲット `ergo_scene_link`
(`include/ergo/scene/editor_link.h`) で、 シーンを publish し、 エディタから
届いたパッチを `Scene::apply_patch` でホストのスレッドに適用する。

シーンモデルを最初から addressable (文字列 id) + シリアライズ可能にしてある
ので、 後から GUI レベルエディタ (配置・追加・削除・保存) へ拡張するときも
モデルを作り直さない。

> `ergo_cast` の `Scene` (Actor を束ねる非所有のグルーピング層) とは別物。
> `ergo_scene` の `Scene` は `ergo::scene` 名前空間のドキュメント。

## カテゴリ
システム

## 所属ドメイン
ルックデヴ / シーン構築 / レベルエディタ基盤

## 必要なデータ
- `*.scene.json` (下記「シーンファイル形式」)
- (`ergo_scene_link` 使用時) `scene_editor` プラグインの接続先
  (既定 `127.0.0.1:5170` / `/scene_editor/ws`) とアプリ名

## 依存
- `ergo_scene`: `ergo_common` のみ (JSON コーデック `ergo::common::jsonm`)
- `ergo_scene_link`: `ergo_scene` + `ergo_common_ws` (共有 WebSocket
  クライアント。 [`common.md`](common.md))。 `ERGO_BUILD_SCENE_LINK`
  (既定 ON) で切り離せる
- C++17 標準ライブラリ (`<vector>`, `<string>`, `<string_view>`, `<memory>`,
  `<functional>`, `<mutex>`)
- (テスト) GoogleTest 互換 mini-gtest

描画系 (`ergo_render` / Pictor) と `ergo_actor` / `ergo_bind` には依存しない。

## データモデル

### Scene
編集対象ドキュメントの単位 (`include/ergo/scene/scene.h`)。

| 要素 | 内容 |
|---|---|
| `id` | シーン id。 `SceneGraph` の mount キー、 `scene_editor` のルーティングキー |
| `domain` / `mount` | 所属ドメインとマウント先 (既定 `"level"` / `"/level"`) |
| `imports` | 参照する他シーンのパス列 |
| `camera` | `Camera` |
| `post_process` | `PostProcess` |
| `actors` | `ActorSpec` の列。 各要素は **文字列 id で addressable** |

主な API: `load_json` / `load_file` / `load(path)` / `save(path)` /
`to_json()`、 `add_actor` (空 id・重複 id は拒否) / `remove_actor` (子の
`parent` を外す) / `clear_actors` / `find_actor`、 `add_import` /
`clear_imports`、 `apply_patch` (下記)。

### ActorSpec
シーンに置かれる 1 アクターの記述。 `ergo_actor::Actor` のインスタンスでは
なく、 純粋なデータ。

| 属性 | 内容 |
|---|---|
| `id` | シーン内一意の文字列 (シリアライズ・エディタ選択のキー) |
| `name` / `type` | 表示名 / 種別名 (既定 `"Actor"`) |
| `parent` | 親アクターの id。 空ならルート (JSON では `null`) |
| `transform` | `pos` (Vec3) / `rot` (クォータニオン `[x, y, z, w]`) / `scale` (Vec3) |
| `visual` | `ResourceRef` (`kind` / `ref` / `material`) |
| `components` | `ComponentSpec` の列。 `type` 以外のプロパティは (キー, JSON テキスト) で保持し、 スキーマを知らずに round-trip する |
| `vars` | `VarSpec` (`name` / `type` / `value`) の列。 `value` は JSON テキスト |
| `instance_of` | プレハブ参照 (JSON キーは `instanceOf`) |
| `overrides` | `OverrideEntry` (`path` / `value` = JSON テキスト) の列。 プレハブ上書き |

### Camera
`mode` (`orbit` / `fly`)、 `target`、 `position` / `front` (fly 用)、
`distance`、 `yaw` / `pitch` (ラジアン)、 `fov_deg`、 `z_near` / `z_far`。
`view_matrix(out)` / `projection_matrix(aspect, out)` で column-major の
float[16] を返す。 入力からカメラを動かす `update` は持たない (操作は
ホストがフィールドを書き換える)。

### PostProcess
`tonemap` (既定 `"none"`)、 `exposure`、 `bloom` (`enabled` / `threshold` /
`intensity`)。 パラメータの入れ物で、 描画パスへの反映はホスト側。

### SceneGraph
複数の `Scene` を id で mount / unmount し、 1 つのアクターグラフとして
見せる実行時コンポジタ。 `mount` (id 空・重複は拒否) / `unmount` /
`is_mounted` / `find_scene` / `find_actor` (mount 順で最初の一致) /
`for_each_actor`。

## パッチ API — `Scene::apply_patch`

`bool apply_patch(const std::string& actor_id, std::string_view field, std::string_view value_json)`

`scene_editor` の wire 形式そのままの 1 フィールド編集を適用する。
`value_json` は新しい値の JSON テキスト。

| field | value | 備考 |
|---|---|---|
| `name` / `type` / `visual.kind` / `visual.ref` / `visual.material` | 文字列 | |
| `transform.pos` / `transform.scale` | 有限数ちょうど 3 個の配列 | |
| `transform.rot` | 数 4 個の配列 `[x, y, z, w]` | 正規化して格納。 長さ 0 は拒否 |
| `vars/<name>/value` | 任意の JSON | 直列化して格納。 `<name>` の var が既に存在すること |
| `parent` | アクター id / `""` / `null` | `""` と `null` は親を外す。 同じシーンに存在し、 自分自身でも子孫でもないこと (循環禁止) |

未知のアクター・未知のフィールド・型や要素数の不一致・不正な JSON・旧形式の
`transform.rot.xyz` は **false を返し、 シーンを一切変更しない**。

## エディタ連携 — `ergo_scene_link`

`ergo::scene::EditorLink` (`include/ergo/scene/editor_link.h`) が
`scene_editor` プラグインのエンジン側クライアント。

- `connect(host = "127.0.0.1", port = 5170, path = "/scene_editor/ws")` /
  `disconnect()` / `is_connected()`。 接続は非同期で、 切れても自動で
  再接続する
- `set_app_name(app)` — エディタに表示されるエンジン名 (既定 `"anonymous"`)
- `publish(scene)` — `{op:"scene", id, json: scene.to_json()}` を送り、
  再接続用に記憶する。 ホスト側でシーンを変えたら再度呼ぶ
- `unpublish(id)` — `{op:"scene_remove", id}` を送り、 記憶とキュー上の
  パッチを捨てる
- 接続が (再) 確立するたびに `{op:"hello", role:"engine", app}` と、
  それまでに publish した全シーンを送り直す
- 受信した `{op:"patch"}` はミューテックス付きのキューに積むだけ (ワーカー
  スレッド)。 `poll(SceneGraph&)` / `poll(Scene&)` がホストのスレッドで
  `apply_patch` を適用し、 **触れたシーンを毎回 publish し直す** (パッチが
  拒否された場合も。 UI 側の値がエンジンの実際の値に戻る)
- `set_transport(hook)` / `receive(frame)` でソケットを差し替えられる
  (テスト用、 または独自トランスポートを持つホスト用)

```cpp
ergo::scene::EditorLink link;
link.set_app_name("lookdev");
link.connect();
link.publish(*scene);
// フレームごと (メインスレッド)
link.poll(*scene);
```

## シーンファイル形式 (`*.scene.json`)

```jsonc
{
  "version": 1,
  "id": "stage_01",
  "domain": "level",
  "mount": "/level",
  "imports": ["prefabs/enemy.scene.json"],
  "camera": {
    "mode": "orbit",
    "target": [0, 0, 0],
    "position": [0, 0, 8], "front": [0, 0, -1],
    "distance": 8.0,
    "yaw": 0.6, "pitch": 0.3,
    "fov_deg": 50, "z_near": 0.1, "z_far": 1000
  },
  "post_process": {
    "tonemap": "aces",
    "exposure": 1.0,
    "bloom": { "enabled": true, "threshold": 1.0, "intensity": 0.6 }
  },
  "actors": [
    {
      "id": "ground",
      "name": "Ground",
      "type": "StaticMeshActor",
      "parent": null,
      "transform": { "pos": [0,0,0], "rot": [0,0,0,1], "scale": [10,1,10] },
      "visual": { "kind": "mesh", "ref": "assets/ground.mesh", "material": "assets/ground.mat" },
      "components": [{ "type": "Collider", "shape": "box" }],
      "vars": [{ "name": "health", "type": "int", "value": 100 }],
      "instanceOf": "prefabs/ground.scene.json",
      "overrides": [{ "path": "vars/health", "value": 50 }]
    }
  ]
}
```

- `actors` が正規のキー。 `objects` は **非推奨の別名**で、 読み込み時に
  `actors` が無ければ受け付ける。 書き出しは常に `actors`
- `components` / `vars` / `overrides` は空なら書き出さない。
  `name` と `instanceOf` も空なら書き出さない
- 省略されたフィールドは既定値になる。 id の無いアクターは読み飛ばす

## 変数
- `Scene`: `id_` / `domain_` / `mount_` / `imports_` / `camera_` /
  `post_process_` / `actors_` (`std::vector<ActorSpec>`)
- `SceneGraph`: mount 済み `std::unique_ptr<Scene>` の列 (mount 順)
- `EditorLink`: WS クライアント、 publish 済みシーンの最新フレーム
  (publish 順)、 アプリ名、 受信パッチキュー (上限 4096 件、 超過時は古い
  ものから捨てる)

## 作業

### 入力
- `Scene::load_json` / `load_file` / `load(path)` — `*.scene.json`
- `add_actor` / `remove_actor` / カメラ・ポストプロセスのフィールド書き換え
  (ホスト)
- `Scene::apply_patch` — フィールド単位の編集
- (`ergo_scene_link`) `scene_editor` から届く `patch` メッセージ

### 出力
- `Scene::to_json()` / `save(path)` — `*.scene.json`
- `Camera::view_matrix` / `projection_matrix`
- (`ergo_scene_link`) `hello` / `scene` / `scene_remove` メッセージ

### タスク
- シーン JSON のパース / 直列化 (`ergo::common::jsonm`)
- アクターの CRUD と親子の整合 (削除時に子を切り離す、 パッチで循環を拒否)
- フリーカメラの行列計算 (view / projection)
- 複数シーンの mount と横断検索 (`SceneGraph`)
- エディタとの往復 (`ergo_scene_link`): publish、 再接続時の再送、 パッチの
  キューイングとメインスレッドでの適用、 適用後の再 publish

## テスト
- `test_scene`: 空シーンの既定値 / load してアクターを id で引ける /
  save → 再 load の round-trip / 重複 id 拒否と削除時の子の切り離し /
  旧 `objects` キーの読み込み / components・vars・instanceOf・overrides の
  round-trip / `SceneGraph` の mount・unmount・横断 / orbit カメラと射影行列
- `test_scene_patch`: フィールドごとに apply_patch → `to_json` → `load_json`
  の round-trip / 回転の正規化 / 未知のアクター・フィールド、 型・要素数の
  不一致、 旧 `transform.rot.xyz` の拒否 (シーン不変) / parent の自己参照・
  不在・循環の拒否と解除 / `SceneGraph::find_scene`
- `test_scene_editor_link` (ソケット不要): 接続時の hello + publish 済み
  シーンの再送 / publish のフレーム内容 / パッチのキューイングと poll での
  適用 / 拒否後も再 publish / `poll(SceneGraph&)` のルーティング /
  `poll(Scene&)` が他シーンのパッチを残す / unpublish / 不正フレームの無視

## 制約 / 先送り
- **描画結線は未実装**。 アクター → drawable 変換と `PostProcess` のパス構成
  への反映は、 `ergo_render` を使うホスト側で行う (将来の結線フェーズ)
- `ergo_actor` / `ergo_bind` への登録 (`bind_var` での公開) は行わない。
  ライブ調整は `scene_editor` プラグイン + `ergo_scene_link` で行う
- エディタからのアクター追加・削除・複製、 ギズモ、 3D ビューポート、
  エディタからの保存は未対応。 現在の `scene_editor` はシーンツリー +
  インスペクタ (フィールド編集) まで
- プレハブ (`instance_of` / `overrides`) は保持と round-trip のみで、
  展開・適用はしない。 パッチで `overrides` を自動記録することもしない
