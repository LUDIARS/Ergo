# 組み込みプラグインにホスト資産を見せる設定

## 目的

組み込み (汎用) プラグインのうち、`render_pipeline` / `visus` / `ui_layout` は
**ホスト側 (Pictor / ゲームプロジェクト) の資産ファイル** を読み書きする。
資産の場所を環境変数で指定するためのガイド。指定しない場合は cwd 相対の既定に
フォールバックせず、そのプラグインのファイル API は **503** を返す
(ヘルスにも出る)。

## 設定キー

| 環境変数 | 対象プラグイン | 役割 / 未設定時 |
|---|---|---|
| `ERGO_PICTOR_PROFILE_DIR` | `render_pipeline` | Pictor の `*.profile.json` を置くディレクトリ。未設定なら API は 503 |
| `VISUS_PROJECT_ROOTS` | `visus` | 描画定義 (`*.visus.json` 等) を走査するプロジェクトルート。`;` 区切りで複数指定可。未設定なら API は 503 |
| `ERGO_UI_LAYOUT_ROOT` | `ui_layout` | `*.uilayout.json` を読み書きしてよいルート。未設定ならファイル API は 503 |

- 読むのは `tools/studio/studio_settings.cpp` だけで、プラグインは環境変数を
  自分で読まない (全変数は [config-reference.md](config-reference.md))。
- `render_pipeline` の保存 (`POST /render_pipeline/api/profile`) は
  `ERGO_PICTOR_PROFILE_DIR` へ書き込む。
- どのプラグインも指定ルート配下に閉じ込めるパス検証を行い、`..` やルート外の
  絶対パスでのエスケープを弾く (共通ガード `tools/studio/plugins/common/safe_path.h` の
  `resolve_within`)。

## 手順

```bash
# ビルド (run-tools-ergo.md)
cmake -S . -B build -DERGO_PACKAGES=pictor -DERGO_PACKAGES_FETCH=ON
cmake --build build --config Release --target ergo_studio

# 例 (PowerShell): Pictor の profile を編集対象にして起動
$env:ERGO_PICTOR_PROFILE_DIR = "E:\Document\Ars\Pictor\profiles"
$env:VISUS_PROJECT_ROOTS     = "E:\Document\Ars\PrivateGame"
build\studio\ergo_studio.exe
```

起動後:

- `render_pipeline`: Studio の `render_pipeline` タブで `*.profile.json` の
  グラフ編集 (エンドポイント詳細は
  [`../feature/tool/render_pipeline.md`](../feature/tool/render_pipeline.md))
- `visus`: Studio の `visus` タブで描画定義の一覧・編集
- `ui_layout`: Studio の `ui_layout` タブで `*.uilayout.json` の編集
  ([`../feature/module/ui_layout.md`](../feature/module/ui_layout.md))

## 注意点

- 旧 `tools/ergo` (2026-11-06 削除) にあった兄弟リポ推定 (`../../../Pictor/profiles`)
  と cwd の既定は無い。配置に関わらず `ERGO_PICTOR_PROFILE_DIR` を明示する。
- これらの env は組み込みプラグイン用で、外部 (ゲーム固有) プラグインの
  ロードに使う `ERGO_PLUGIN_DIR` ([external-plugins.md](external-plugins.md))
  とは別物。
