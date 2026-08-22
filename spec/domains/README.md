# ドメイン定義 (spec/domains)

Anatomia が参照するドメイン定義の **正本**。 1 ドメイン = 1 ファイルで
`<ドメイン名>.domain.json` を置く。 `.anatomia/` はツールが生成するローカル
状態にすぎず (`.gitignore` 済み)、 手で編集しても他の開発者には伝わらない。

## ファイル形式

```json
{
  "name": "<ドメイン名 (ファイル名と一致させる)>",
  "description": "<日本語 1〜2 文。 このドメインが持つ責務>",
  "presetRules": [],
  "templateRules": [],
  "membership": [
    { "pathPattern": "(^|/)src/<モジュール>/(?:.*/)?[^/]+$" },
    { "pathPattern": "(^|/)include/(?:.*/)?<モジュール>/(?:.*/)?[^/]+$" }
  ]
}
```

- `pathPattern` は正規表現。 先頭を `(^|/)` にして、 リポジトリ相対でも
  絶対パスでも同じように当たるようにする
- C++ モジュールは `src/<名>/` と `include/ergo/<名>/` の 2 本を必ず対で書く
  (片方だけだとヘッダと実装が別ドメインに割れる)

## ルール

- **1 パスは 1 ドメイン**。 複数のドメインに当たるパターンを書かない。
  特にディレクトリ名を後方一致で拾うパターン (`(^|/)src/ui/...` など) は
  `tools/` 配下の同名ディレクトリを巻き込みやすいので注意する
- **新規ソースディレクトリを足す PR は、 同じ PR で membership を更新する**。
  どのドメインにも属さないコードは Anatomia の解析から漏れる
- モジュールの割り当ては `spec/module/<名>.md` の「所属ドメイン」と矛盾させない。
  食い違う場合は、 どちらが正しいかを決めて両方を直す
- ドメイン名は kebab-case。 リネームは Anatomia 側の履歴と突き合わせが切れるため、
  必要なとき以外はしない

## 現在のドメイン

| ドメイン | 主な範囲 |
| --- | --- |
| `foundation` | 共通型・ユーティリティ・ビルド定義・`tools/`・`tests/`・`benchmarks/` |
| `actor-character` | `actor` / `character` / `cast` |
| `ai-behavior` | `blackboard` |
| `audio` | `audio` / `sound` |
| `combat-skill` | `combo_counter` / `health` / `score` / `timing_judge` |
| `input` | `input` |
| `observability` | `log` / `profile` / `custos` / `bind` |
| `physics` | `physics2d` / `math` |
| `rendering` | `render` / `particle` / `gpu_particle` / `shuriken_migrator` / `shaders` |
| `resource-io` | `resource` / `io` / `http` |
| `scene-frame` | `scene` / `frame` |
| `ui-layout` | `ui` / `ui_layout` / `ui_kit` / `vector` |
| `vendored-third-party` | `third_party` |
| `world-time` | `world_time` |
