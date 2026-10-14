# ドメイン定義 (spec/domains)

Anatomia が参照するドメイン定義の **正本**。 1 ドメイン = 1 ファイルで
`<ドメイン名>.domain.json` を置く。 `.anatomia/` は原則としてツールが生成する
ローカル状態だが、 プログラム層の宣言である `.anatomia/layers.json` だけは
リポジトリで追跡し、 他の開発者や Revisor と共有する。

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
  絶対パスでも同じように当たるようにする。 ただしリポジトリ直下だけを指すファイルやツール
  (`CMakeLists.txt`・`ergo-packages.json`・`CMakePresets.json`・`.gitmodules`・`tools/ergo/`) は
  `^` で始め、 テスト用フィクスチャの中にある同名ファイルを巻き込まない
- C++ モジュールは `src/<名>/`・`include/ergo/<名>/`・`tests/<名>/` の 3 本を必ず組で書く
  (欠けるとヘッダ・実装・テストが別ドメインに割れる)。 `include/` は `include/ergo/<名>/` まで
  書き切り、 `include/(?:.*/)?<名>/` のような途中一致にしない

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

2026-10-14 に 14 本から 16 本へ切り直した (**AI 案・人の承認前**)。 理由・旧ドメインとの対応・
Praeforma のコア / ビジネスドメインとの対応・古い処理の一覧は
[spec/architecture/domains-and-ux.md](../architecture/domains-and-ux.md) を参照。

| ドメイン | 主な範囲 |
| --- | --- |
| `foundation` | `common` / `math`・`template/`・直下の `CMakeLists.txt`・`benchmarks/`・`tests/gtest_main.cpp`・`tests/thirdparty/` |
| `build-packages` | `cmake/`・`ergo-packages.json`・`CMakePresets.json`・`tests/packages/`・`lib/`・`.gitmodules` |
| `structure-visualization` | `graph_layout` / `tela_graph` / `render_viz` / `package_model` / `package_scan` / `package_viz`・`tools/render_viz/`・`tools/package_viz/` |
| `observability` | `log` / `frame` / `profile` / `bind` / `custos` |
| `dev-tools` | `tools/ergo/` |
| `input` | `input` |
| `physics` | `physics2d` |
| `audio` | `audio` / `sound` |
| `rendering` | `render` / `particle` / `gpu_particle`・`shaders/` |
| `ui-layout` | `ui` / `ui_kit` / `ui_layout` / `vector` |
| `resource-io` | `resource` / `io` / `http` |
| `actor-character` | `actor` / `character` |
| `scene-composition` | `scene` / `cast` |
| `gameplay-rules` | `health` / `score` / `combo_counter` / `timing_judge` / `world_time` / `blackboard` |
| `vendored-third-party` | `third_party/` |
| `legacy` | `shuriken_migrator`・`tools/shuriken_migrator/`・`review/`・`.anatomia/domains/` (削除するかを人が決めるまで分けて見せる) |

モジュール名だけの項目は `src/<名>/`・`include/ergo/<名>/`・`tests/<名>/` を指す。

## legacy ドメイン

`legacy` には、使われ方が細っている・置き換え済み・旧運用の名残と判断したコードを置く。
入れてもビルドと動作は変わらない。 人が「残す」と決めたら元のドメインへ戻し、
「消す」と決めたら削除 PR で membership も一緒に外す。 ファイル単位の名残は
除外パターンで割らず、 設計文書の一覧だけで示す (1 パス 1 ドメインを崩さないため)。
