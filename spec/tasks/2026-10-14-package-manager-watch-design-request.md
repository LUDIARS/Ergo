# 設計依頼: パッケージマネージャ・更新の定期確認と通知・Ludellus-Native の状態確認・Unity パッケージ

日付: 2026-10-14
対象: Ergo (追加で Unity 向けパッケージの置き場所も設計対象)

## 1. 進め方

- 同じ依頼文で 2 本 (Fable / Opus) に別々に設計を書いてもらう。互いの成果は見ない。
- 親セッションが 2 案を比較し、統合設計を 1 本にまとめて確定する。
- この依頼は設計だけ。実装はしない。

## 2. 人間の指示 (原文)

> Ergoはパッケージマネージャも兼ね、またPfやAIによるブランチ更新を定期的に確認して更新を通知するようにします。
> 現在作成しているLudellus-Nativeの管理や状態を確認できるようにしましょう。
>
> この通知部分とパッケージマネージャについては、Unity用のパッケージも用意します。

直前の関連指示 (設計済み・実装中):

> Ergoがコンテンツの各LUDIARSのパッケージをまとめてリンク/管理するようにします。各パッケージのバージョン、何をリンクしているか、MUSAや関連パッケージは何を使用しているかなどを可視化します。可視化はTelaで行い、今回のパイプライン可視化もErgoでやります。

## 3. 親の解釈 (違うと思えば指摘すること)

1. **パッケージマネージャ**: 現在のパッケージ・ハブ (`spec/feature/package-hub.md`) は「宣言 (ergo-packages.json) → 解決 → リンク → 可視化」まで。
   これを、ホストがパッケージを **追加・削除・更新 (pin の変更)・取得** できる道具に広げる。
   例: `ergo pkg add/remove/update/outdated/fetch/lock`。lock ファイル (解決済みの sha とタグ) を持つかどうかも論点。
   既存の取り込み経路 6 通り (S2) と pin 規則、STRICT のタグ要求 (N4)、pin の更新はホスト側 (N7) と矛盾しないこと。
2. **ブランチ更新の定期確認と通知**: ホストが依存しているパッケージのリポで、Praeforma (Pf) や AI (Concordia の委託など) が
   ブランチを更新・main へマージしたことを定期的に検知し、「どのホストのどの依存に、どんな更新が来たか
   (新しいタグ / main の進み / 審査中のブランチ)」を通知する。
   - Pf が実際にどうブランチを作る・更新するかは親も確かめていない。`E:/Document/Ars/Praeforma` を読んで確かめること。
   - AI の更新は Concordia の委託ブランチと Revisor の local PR が主。
   - 通知先 (Discord / Concordia / エディタ内 / CLI 出力) と、定期実行の担い手 (Ergo の常駐か、Excubitor のジョブか、CI か、
     エディタ起動時か) は論点。Ergo は C++ ライブラリ群なので、常駐サービスを Ergo に持たせるのが適切かも判断すること。
3. **Ludellus-Native の管理・状態確認**: Ludellus-Native (`E:/Document/Ars/Ludellus-Native`、C++/CMake/Vulkan、Ergo と Pictor に依存)
   を最初の実利用ホストとして、使っているパッケージの版・遅れ (outdated)・来ている更新・ビルドとテストの状態などを
   1 か所で見られるようにする。表示は Tela (既存の ergo_package_viz / TELA_GRAPH の延長) を想定。
   Ludellus-Native はまだ ergo-packages.json を持たない (現状は `SUIKA_ERGO_DIR` などの独自変数)。移行 (S5) との関係も扱うこと。
4. **Unity 用パッケージ**: 通知部分とパッケージマネージャを Unity からも使えるように、UPM パッケージ (Unity Package Manager 形式) を用意する。
   Unity のプロジェクトは CMake を使わないので、何を共有し何を Unity 側で持つか (同じ宣言ファイル形式か、C# で読むか、
   ネイティブ CLI を呼ぶか) が論点。置き場所 (Ergo 内の `unity/` か、UnityFoundation (Uf) か、新規リポか) も決めること。

## 4. 前提として読むもの

- Ergo: `spec/feature/package-hub.md` (統合設計。§7 の段階 S0〜S7、未決 N1〜N8)、`spec/plan/2026-10-14-package-hub-{fable,opus}.md`
- Ergo: `ergo-packages.json`、`cmake/packages/`、`include/ergo/{package_model,package_viz,graph_layout,tela_graph}/`、`tools/package_viz/`
- Ergo: `CLAUDE.md` (SRP・ファイル分割、モジュール間 cross-import 禁止)、`spec/module/module_list.md`
- Ludellus-Native: `README.md`、`native/CMakeLists.txt`、`spec/setup/native-build.md`、`docs/PICTOR.md`
- Praeforma: `spec/feature/praeforma.md` ほか (ブランチの作り方・更新の仕方を確かめる)
- UnityFoundation: `E:/Document/Ars/UnityFoundation` (既存の Unity 向け資産の置き方)
- 状態の取得元の候補: Revisor の local PR API (`http://127.0.0.1:4240/v1/local-prs/<id>`)、Concordia の委託 run、git (ls-remote / fetch)、GitHub のタグ

## 5. 設計してほしいこと (独立に考えてよい。解釈と違う結論になってよい)

1. パッケージマネージャの範囲: コマンド体系、lock ファイルの要否と形式、ergo-packages.json との関係、取得 (fetch) の方法と置き場所、
   オフライン時・兄弟チェックアウト時の振る舞い、既存の S2 経路・STRICT・N7 との整合。
2. 更新の検知: 何を「更新」とみなすか (タグ / main の進み / 審査中ブランチ / Pf 由来 / AI 由来の区別)、取得元と認証、頻度、
   前回状態の保存場所、重複通知の抑止。Pf の実際の挙動を確かめた結果を書くこと。
3. 通知: 通知先と形式、誰に届くか、通知の内容 (どのホスト・どの依存・何が変わったか・更新に必要な操作)、
   定期実行の担い手 (常駐を Ergo に持たせるか、外部に任せるか) とその理由。
4. Ludellus-Native の状態確認: 何を表示するか、データの取り方、Tela での画面構成 (既存の P1/P2 図との関係)、
   Ludellus-Native の ergo-packages.json 化 (移行手順・既存の独自変数の扱い)。
5. Unity パッケージ: 形式 (UPM)、置き場所、Unity 側で持つ機能 (エディタ窓・通知・パッケージ一覧と更新)、ネイティブ側との共有のしかた、
   Unity 版の検証方法。
6. 既存段階 (package-hub S4〜S7) との関係: 今の段階表のどこに入れるか、段階の組み直し。S4 (観測 P3: package.json / .gitmodules /
   ported_from の scan、MUSA 図) は並行して実装が進む前提で、衝突しない形にすること。
7. 公開リポとしての注意: Ergo は公開リポ。開発機の絶対パス・秘匿語・トークンを入れない設計にすること (通知の本文も含む)。
8. 実装の分割と受入条件 (段階ごと・機械で確かめられる条件つき)。

## 6. 成果物

- 出力パス: Fable は `spec/plan/2026-10-14-package-manager-watch-fable.md`、Opus は `spec/plan/2026-10-14-package-manager-watch-opus.md`
- 章立て: 結論 → 解釈 (親の解釈との違い) → 比較した案 → 推奨案の詳細 (1〜7) → 実装の分割と受入条件 → 未決事項 (推奨つき)
- 実装しない。設計書以外のファイルを変更しない。PR を出さない。もう一方の案は見ない。
- 書き終えたらコミットし、要点 5 行を報告してから終了指示を待つ。
