# scene_editor plugin — シーンツリー + インスペクタ + パッチ中継

## 概要

Ergo 統合開発者ツール (`tools/ergo`) の組み込みプラグインの 1 つ。
エンジン (ホストアプリ) が publish した `ergo_scene` のシーンをブラウザに
シーンツリーとして表示し、 選んだアクターのフィールドをインスペクタで編集
する。 編集はフィールド単位の `patch` として **シーンを publish した
エンジンにだけ**中継され、 エンジンが適用して publish し直した結果が UI に
戻る。 UI は自分のコピーを書き換えないので、 表示は常にエンジンの実際の
状態になる (エンジンが拒否した編集は元の値に戻る)。

リポジトリ内のエンジン側クライアントは `ergo_scene_link`
(`include/ergo/scene/editor_link.h`、 [`spec/feature/module/scene.md`](../module/scene.md))。
ホストはソケットや JSON のコードを書かずに接続できる。

## 設置場所

- 実装: `tools/ergo/src/plugins/scene_editor/`
  - `index.ts` — plugin definition (Hono routes + WS 中継)
  - `protocol.ts` — wire protocol の型
  - `parse_scene.ts` — エンジンの `Scene::to_json()` → UI 用 `SceneInfo` 変換 (純関数)
  - `ui/index.html` / `ui/styles.css` / `ui/app.js` — UI 本体 (ES module)
  - `ui/rotation.js` — オイラー角 (度) ↔ クォータニオン変換
- 登録: `tools/ergo/src/core/registry.ts` の `PLUGIN_FACTORIES`
- テスト: `tools/ergo/tests/plugins/scene_editor/`
  (`parse_scene` / `relay` / `rotation`)

## URL

| URL | 役割 |
|---|---|
| `http://localhost:5170/scene_editor/` | UI |
| `http://localhost:5170/scene_editor/api/scenes` | 現在保持しているシーン (`SceneInfo[]`) |
| `http://localhost:5170/scene_editor/api/health` | プラグインヘルス (`scenes` / `clients`) |
| `ws://localhost:5170/scene_editor/ws` | エンジン / UI 共通の WS 入口 |

## プロトコル (WS)

すべて JSON テキストフレーム。 接続の役割は最初の `hello` で決まり、
`hello` を送っていない接続のフレームは無視する。

### エンジン → サーバ

```json
{ "op": "hello", "role": "engine", "app": "lookdev" }
```

```json
{ "op": "scene", "id": "stage_01", "json": "<Scene::to_json() のテキスト>" }
```

```json
{ "op": "scene_remove", "id": "stage_01" }
```

- `scene` は全体スナップショット。 同じ id を再送すると置き換える。
  `id` (フレーム側) がシーンのキーで、 UI のパッチもこの id で届く
- `json` が JSON として読めないフレームは無視する

### UI → サーバ

```json
{ "op": "hello", "role": "ui" }
```

```json
{ "op": "patch", "scene_id": "stage_01", "actor_id": "crate",
  "field": "transform.pos", "value": [1, 2, 3] }
```

### サーバ → UI

```json
{ "op": "scenes", "scenes": [ <SceneInfo>, ... ] }
```
(`hello` 直後のスナップショット)

```json
{ "op": "scene_update", "scene": <SceneInfo> }
```

```json
{ "op": "scene_removed", "id": "stage_01" }
```

`SceneInfo` は `protocol.ts` の型。 アクターごとに `id` / `name` / `type` /
`parent` (ルートは `""`) / `transform` (`pos` / `rot` = `[x, y, z, w]` /
`scale`) / `visual` / `instanceOf` / `overrides` / `vars` / `components` を
持つ。 `vars[].value`・`overrides[].value`・`components[].props` の値は
**JSON テキスト** (UI はこれを JSON として編集して送り返す)。

### サーバ → エンジン

UI の `patch` を、 `scene_id` のシーンを publish したエンジン接続にだけ
そのまま転送する。 該当シーンが無ければ捨てる。

## フィールドパス

エンジンは `Scene::apply_patch` で適用する。 受け付けるパス:

| field | value | UI での編集 |
|---|---|---|
| `name` / `type` | 文字列 | テキスト |
| `parent` | アクター id、 `""` で親を外す | テキスト |
| `visual.kind` / `visual.ref` / `visual.material` | 文字列 | テキスト |
| `transform.pos` / `transform.scale` | `[x, y, z]` | 数値 3 つ |
| `transform.rot` | クォータニオン `[x, y, z, w]` | オイラー角 (度) 3 つ。 送信時にクォータニオンへ変換 |
| `vars/<name>/value` | 任意の JSON | JSON テキスト (文字列は引用符付き) |

- オイラー角は X → Y → Z の順にワールド軸まわりに回す (`q = qz · qy · qx`)
- エンジンは回転を正規化して格納する。 parent は同じシーンに存在し、 自分
  自身でも子孫でもないこと
- 旧形式の `transform.rot.xyz` (オイラー xyz) は受け付けない。 それ以外の
  未知パス・型違いも拒否され、 エンジンが変更前のシーンを publish し直す

## ライフサイクル

1. エンジンが接続して `hello` (role `engine`) を送り、 続けて publish 済みの
   全シーンを `scene` で送る。 `ergo_scene_link` は **接続が確立するたびに**
   これを自動で行うので、 `tools/ergo` を再起動してもホスト側の作業は不要
2. UI が接続して `hello` (role `ui`) を送ると、 保持中の全シーンを `scenes`
   で受け取る。 以後 `scene_update` / `scene_removed` を受け取る
3. UI の `patch` は所有エンジンへ転送される。 `ergo_scene_link` はそれを
   キューに積み、 ホストが `poll()` したときにメインスレッドで適用し、
   触れたシーンを publish し直す (拒否された場合も)。 その `scene` が
   `scene_update` として UI に届く
4. エンジン接続が閉じると、 その接続が publish したシーンをすべて削除し、
   UI に `scene_removed` を送る

## 既知の制限

- アクターの追加・削除・複製、 ギズモ、 3D ビューポート、 UI からの保存は
  未対応 (シーンツリー + インスペクタのみ)
- カメラとポストプロセスは表示も編集もしない
- `scene_update` を受けるたびにインスペクタを描き直すため、 編集中の
  入力欄は別のエンジン更新で上書きされることがある
- 同じシーン id を 2 つのエンジンが publish すると、 後から送った方が所有者
  になる
