# 設計依頼: Ergo による LUDIARS パッケージの統合リンク・管理と Tela 可視化

- 日付: 2026-10-14
- 対象: Ergo (Eg)。可視化は Tela (Tl)

## 0. 進め方 (二本立て)

この依頼文は 2 者 (Fable と Opus) へ同じ内容で別々に渡している。**もう一方の案は見ないこと。**
親セッションが 2 案を突き合わせて統合設計を 1 本にする。あなたの役目は独立した 1 案を書くこと。

## 1. 人間の指示 (原文)

- neco 2026-10-14:「Pictorのレンダリングパイプラインの可視化を強化する。ErgoレイヤーにTelaを使った可視化ツールを用意する。」
- neco 2026-10-14:「Ergoがコンテンツの各LUDIARSのパッケージをまとめてリンク/管理するようにします。
  各パッケージのバージョン、何をリンクしているか、MUSAや関連パッケージは何を使用しているかなどを可視化します。
  可視化はTelaで行い、今回のパイプライン可視化もErgoでやります。」

## 2. 親の解釈 (違うと思えば指摘すること)

- 「コンテンツ」はゲームなどの利用側ホスト (AdventureCube、KonbiniDominant、Ludellus-Native など)。今は各ホストが
  Ergo・Pictor・Tela・Lapilli などを個別に `add_subdirectory` / 取り込みしている。これを **Ergo を 1 つの入口にして**、
  ホストは Ergo だけを取り込めば、必要な LUDIARS パッケージ (C++ ライブラリ) が決まった版で揃う形にする。
- 「管理」は、どのパッケージをどの版 (コミット / タグ / Revisor の版) で使うかを Ergo 側の宣言 (マニフェスト) で持ち、
  ホストごとの選択 (使う / 使わない) を受け付けること。版の食い違いや足りない依存を構成時に検出する。
- 「可視化」は、パッケージの版・リンク関係 (どのターゲットが何にリンクしているか)・MUSA 系や関連パッケージが何を使っているか
  を関係図にし、Tela で表示すること。描画パイプライン可視化と同じ出口を使う。
- 「MUSA」は LUDIARS の MUSA (判断の移譲の枠組み。女神名のプロジェクト群: Clio / Thaleia / Terpsichore など、および
  Ars-Musa プラグイン) を指すと解釈する。Ergo の `ergo_resource` は MUSA Clio の C++ 移植。どこまでを可視化対象に含めるか
  (C++ パッケージだけか、サービスや TS パッケージも含むか) は論点として扱う。
- Pictor から Ergo への依存は作らない (Pictor AGENTS.md: Ergo は主な消費者だが Pictor の必須依存にしない)。
  Ergo がパッケージ群の「上に立つ集約点」になり、下層パッケージは Ergo を知らない。

## 3. 前提として読むもの (すべて絶対パス)

Ergo (main と同内容の作業場所 `E:/Document/Ars/Ergo/.worktrees/package-hub-design`):
- `CLAUDE.md`、`README.md`、`module_list.md`、`module_list.yaml`、`CMakeLists.txt`、`CMakePresets.json`、`cmake/`
- `spec/domains/README.md`、`spec/tool/ergo.md` (Web ツール `tools/ergo/` のプラグイン方式)
- `cmake/ErgoRenderBackend.cmake` (今の Pictor の取り込み方: ホストが `pictor` ターゲットを用意する前提)

可視化の既存部品 (Ergo #2904 で main にマージ済み、コミット 39f1efa。main 起点の作業場所なら直接読める):
- `include/ergo/tela_graph/graph.h`、`include/ergo/tela_graph/writer.h`、`include/ergo/tela_graph/tela_adapter.h`
  (Ergo の可視化の共通出口。配置済みの関係図 → Tela `TELA_GRAPH 1` / `tela::Graph`)
- `spec/module/tela_graph.md`、`spec/module/render_viz.md` (描画パイプライン可視化。最初の利用者)

Tela:
- `E:/Document/Ars/Tela/README.md`、`E:/Document/Ars/Tela/spec/contracts.md` (SPEC-TL-GRAPH: 上限 8 グループ / 128 ノード /
  256 辺。Tela はレイアウトしない)、`E:/Document/Ars/Tela/spec/feature/graph-view.md`、`E:/Document/Ars/Tela/spec/feature/view-host.md`

パッケージ側の例 (取り込み方・版の持ち方を見る):
- `E:/Document/Ars/Pictor/CMakeLists.txt`、`E:/Document/Ars/Pictor/AGENTS.md`
- `E:/Document/Ars/Tela/CMakeLists.txt` (`find_package(Tela CONFIG)` と in-tree 両対応、兄弟ソースを暗黙に取りに行かない方針)
- `E:/Document/Ars/Lapilli/`、`E:/Document/Ars/Ars-Musa/`、`E:/Document/Ars/Clio/`、`E:/Document/Ars/Thaleia/`、`E:/Document/Ars/Terpsichore/`
- LUDIARS の略称表: `E:/Document/Ars/LUDIARS/PROJECT-CODES.md`
- 版の正本: Revisor が各リポの版を持つ (`.revisor-version`、`revisor version show`)。

並行している関連設計 (Pictor の描画方式の切り替え口): `E:/Document/Ars/Pictor-design-multi-rhi/spec/tasks/2026-10-14-multi-rhi-design-request.md`
(Pictor のビルド構成が方式ごとの任意モジュールに分かれる予定。パッケージの「使う機能の選択」に影響する)

## 4. 設計してほしいこと (すべて扱う。独立に考えてよく、親の解釈と違う結論でもよい)

1. **対象範囲**: どのパッケージを Ergo の管理下に置くか (C++ ライブラリ / シェーダ・資産 / ツール / サービス / TS パッケージ)。
   MUSA 系をどう位置づけるか。
2. **マニフェスト**: パッケージ・版・取得元・任意機能 (Pictor の描画方式など) の宣言形式と置き場所。ホストごとの選択と上書き。
   ロック (再現性) の扱い。既存の Revisor 版との関係 (二重管理にしない)。
3. **取り込みの仕組み**: CMake でホストが Ergo だけを取り込めばよい形 (add_subdirectory / FetchContent / find_package /
   ローカル兄弟チェックアウトの扱い)。「依存を暗黙に取りに行かない」既存方針 (Tela / Pictor) との両立。オフライン・CI・
   worktree での動作。Emscripten (Ludellus-Native の WebAssembly 化) でも同じ仕組みが使えること。
4. **検査**: 版の食い違い・足りない依存・循環・方式の組合せ不正を構成時に検出して止める規則 (黙って通さない)。
5. **リンク関係の収集**: CMake のターゲット依存 (どのターゲットが何にリンクしているか) と、パッケージ間の使用関係
   (例: MUSA 系が何を使っているか) をどう機械的に集めるか (CMake File API / graphviz 出力 / 宣言 / ソース解析)。
   手書きの図にしない。
6. **可視化**: 図の種類 (パッケージ版一覧・リンク関係・MUSA の使用関係など) と、Tela の上限 (8 グループ / 128 ノード / 256 辺)
   の中に収める分け方・畳み方。配置の規則。`ergo_tela_graph` を出口として使い、描画パイプライン可視化と並ぶ形
   (モジュール / CLI / ツールの切り方)。Tela で表示する手順。
7. **Ergo の中での配置**: モジュール構成 (Ergo の規約: 1 モジュール 1 責務、モジュール間 cross-import 禁止、共有は下層へ)、
   既存 `ergo_render` が Pictor を受け取る今の形との整合、Web ツール (`tools/ergo/`) との関係。
8. **既存ホストの移行**: AdventureCube などが今の取り込み方から移る手順と互換期間。
9. **段階計画と受入条件**: 各段階が単独で main に入れられる分け方。何をテストし、何を実機で確かめるか
   (実機・実ホストで未確認のものは未確認と書く前提)。
10. **リスクと未決事項**: 推奨つきで。人間 (neco) にしか決められない点は分けて書く。

## 5. 成果物

- 出力先 (自分の側だけ):
  - Fable: `spec/plan/2026-10-14-package-hub-fable.md`
  - Opus: `spec/plan/2026-10-14-package-hub-opus.md`
- 章立て: 結論 → 解釈 (親の解釈への同意・異論) → 比較した案 → 推奨案の詳細 (上の 1〜8) → 段階計画と受入条件 (9)
  → リスク → 未決事項 (推奨つき) (10)。
- **実装しない。設計書以外のファイルを変更しない。**
- 書き終えたらコミットし、要点を 5 行で報告して、終了の指示を待つ。**PR は出さない。**
- Ergo は公開リポジトリ。非公開タイトル名・社内の固有名を設計書に書かない。
