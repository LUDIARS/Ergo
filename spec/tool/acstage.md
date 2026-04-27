# acstage — AdventureCube stage editor

Ergo の統一開発者ツール (`tools/ergo`) にホストされるプラグインの
1 つ。URL は `http://localhost:5170/acstage/`。

`placer` と違って **AdventureCube 専用** (=AC のステージ JSON スキーマに
密着した dedicated エディタ)。AC の `data/master_data/stages/<id>.json` を
直接 read / write し、編集後は **AC を再起動するだけで反映** される。
現状 AC 側に hot-reload は無いので WS は将来用に空フックだけ確保。

## 目的

AC の Stage を構成する 3 種のデータ (`fields[]` / `placements[]` /
`enemies[]`) を統一フォーマット 1 ファイルで編集する。`placer` の
ブロックグリッド抽象とは設計が違う (AC は world-coord 直書き) ため、
互換変換ではなく独立 plugin にした。

## データモデル

1 stage = 1 JSON ファイル (`<stage_id>.json`)。スキーマ詳細は
`AdventureCube/spec/gamedesign/Stage.md` と
`AdventureCube/spec/gamedesign/Combat.md` を参照。

```jsonc
{
  "stage_id":   "stage-01",
  "stage_seed": 2914517694,
  "fields":     [ { "id":"f_01", "category":"grass", "note":"start" } ],
  "placements": [ { "instance_id":"demo-01", "position":[1.5, 0.35, 0.0] } ],
  "enemies":    [ { "instance_id":"grunt-01", "type":"grunt",
                    "position":[6.0, 0.5, 0.0],
                    "attack":{ "damage":5, "reach":1.0, "cooldown":1.5 } } ]
}
```

## 保存先

```
ERGO_ACSTAGE_DIR > probe(./data/master_data/stages, ../...) > ./data/master_data/stages
```

`editor.bat` (AC 同梱) は `ERGO_ACSTAGE_DIR=<repo>\data\master_data\stages`
を起動前にセットしているので、AC リポジトリ直下から `editor.bat` を
叩けば即繋がる。

## REST

すべて `/acstage/api/*` 配下:

| Method | Path | 説明 |
|---|---|---|
| GET | `/meta` | `{ schema_version, dir, exists, stages, allowed_categories, allowed_enemy_types }` |
| GET | `/stages` | `{ stages:[ { stage_id } ] }` |
| GET | `/stages/:id` | `{ ok, stage }` (normalised) |
| PUT | `/stages/:id` | upsert 1 stage (body = full StageFile JSON) |
| POST | `/stages/:id` | 新規空 stage (body 空) または `{from:"<src_id>"}` で複製 |
| DELETE | `/stages/:id` | 1 stage 削除 |

入力は `normaliseStageFile` で AC スキーマ準拠に整形してから atomic
write (`*.tmp` → rename)。不明なフィールドは捨てる (拡張時はまず schema
を増やす)。

## UI

シンプルな 1 ページ:
- ヘッダ: stage 選択 / New / Duplicate / Delete / **Save**
- 左ペイン: stage JSON のテキスト編集 + Format JSON / + field / + placement / + enemy
- 右ペイン: 件数サマリ + 編集ルールの早見

巨大なフォーム UI は意図的に作らない (JSON 直編集の方が AC 仕様変更に
追従しやすい + ステージ数が少ないため)。

## WS

`/acstage/ws` はハンドシェイクのみ実装。AC に live-reload を入れる際は
ここから `{ op:"reload", stage_id }` を push する想定。
