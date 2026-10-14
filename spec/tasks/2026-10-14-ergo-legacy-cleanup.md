---
task: Ergo の古い処理の片付け (Ergo 内で完結する分)
project: Eg
kind: 実装
created: "2026-10-14"
source_session: lictor-f6cd21b6-09b4-4de4-abc7-af88447edf90
---
# 2026-10-14 Ergo の古い処理の片付け (Ergo 内で完結する分)

- 起点: Actio `actio:13f75909-828d-41a8-9f56-7a1d3157d9f4`
- 一覧の正本: [spec/architecture/domains-and-ux.md](../architecture/domains-and-ux.md) §5 (人の判断: 推奨どおり進める)
- ブランチ: `chore/ergo-legacy-cleanup`
- 範囲: L3 / L4 / L5 / L6 / L7 / L9。L1・L2・L8 は移行先のリポでの作業が先なので扱わない

## 実装内容

設計文書 §5 の古い処理の一覧のうち、Ergo の中で完結する 6 件を片付ける。

| # | 作業 | 成果物 | 状態 |
| --- | --- | --- | --- |
| 1 | L3: `ergo/bind/json_min.h` の利用 2 か所を `ergo/common/json_min.h` に直し、シムを削除 | `src/bind/bind_engine.cpp`・`tests/bind/test_json_min.cpp` | 済 |
| 2 | L4: `.anatomia/domains/render-orchestration.json` を削除 | — | 済 |
| 3 | L5: `review/` を削除 (記録はリポ外のレビュー管理先に同じ日付で残っていることを確認) | — | 済 |
| 4 | L6: 利用側の CMake を確認し、kazmath の取り込み・プリセット・疎通テスト・通知・セットアップ文書を外す | `third_party/dependencies.cmake`・`CMakePresets.json`・`tests/thirdparty/`・`spec/setup/` | 済 |
| 5 | L7: 利用側を確認し、`ERGO_BUILD_DUMMY` の既定を OFF に | `CMakeLists.txt`・`spec/setup/` | 済 |
| 6 | L9: `module_list.md` / `.yaml` に `ergo_ui_kit` を足し、仕様書の列を実物の置き場所にそろえる。`ergo_shuriken_migrator` は載せない理由を注記 | `module_list.*`・`spec/module/ui_kit.md` | 済 |
| 7 | `legacy` の membership から消したものを外し、§5 に結果を書く (`anatomia domains program --repo .` で unclassified 0) | `spec/domains/`・設計文書 §5 | 済 |
| 8 | 審査の所見 (公開できない語: local-path) への対応。オフライン手順の例示パスを記号 (`<curl-src>` / `<deps>/curl`) に置き換え | `spec/setup/third-party-deps.md`・`spec/setup/ide-projects.md` | 済 |


### 変更した境界

- 公開ヘッダ `ergo/bind/json_min.h` を削除した。`ergo::bind::jsonm` を使っていた外部コードは `ergo/common/json_min.h` と `ergo::common::jsonm` に直す必要がある (利用側のリポには参照が無いことを確認済み)。
- CMake: `ERGO_WITH_KAZMATH` と `kazmath` ターゲットが無くなった。`ERGO_BUILD_THIRDPARTY_SMOKE` は curl だけを強制する。`ERGO_BUILD_DUMMY` の既定は ON から OFF に変わった。
- プリセット `vs2022-deps` / `xcode-deps` は libcurl だけを取り込む。

### 復旧方法

- 本 PR の squash コミットを revert すればすべて戻る。部分的に戻すなら、kazmath は `cmake/deps/kazmath.cmake` と `third_party/dependencies.cmake` の宣言・スイッチを戻す。dummy を使うホストは `-DERGO_BUILD_DUMMY=ON` を指定すればよい。

### 再利用探索

- json_min は既存の `ergo/common/json_min.h` をそのまま使った (新しい実装は無し)。
- `ergo_ui_kit` の仕様書は `template/module_template.md` の節立てに合わせ、公開ヘッダとテストから書いた。

## 受け入れ条件

- L3・L4・L5・L6・L7・L9 の各項目が片付き、設計文書 §5 の表に結果 (済み・見送りと理由) が書かれている。
- L1・L2・L8 と、パッケージマネージャのファイルには手を入れていない。
- `legacy` の membership から消したパスが外れ、`anatomia domains program --repo .` の unclassified が 0 のまま。
- 追加した文章に秘匿語と開発機の絶対パスが無い。

### 検証

- 実施: `anatomia domains program --repo .` (unclassified 0)、秘匿語辞書との照合 (0 件)、利用側のリポの CMake の確認 (kazmath・dummy の参照無し)。
- 未実施: ビルドとテスト (委託の指示により実行しない)。`test_json_min` と `test_thirdparty_deps` の変更は、次のビルドで確かめる。
