---
title: Ergo Studio のネイティブ化 (Electron 廃止) と MUSA プラグイン
type: plan
status: in_progress
service: ergo
domain: tools
updated: 2026-11-05
---

# Ergo Studio のネイティブ化 (Electron 廃止) と MUSA プラグイン

- Actio: `actio:924f0113-8fb5-44ea-9680-d65ac8b233eb`
- branch: `feat/ergo-studio-native`

## 1. 依頼

人間の指示 (2026-11-05):

1. 「Ergo Studio は Electron を使用せず C++ と UI エンジンのみで対応する」
2. 範囲は「全部 C++ に」 — studio の 6 タブだけでなく、`tools/ergo` の全プラグインを C++ にする
3. 「MUSA は未実装だったら実装もする」
4. 「Ergo の Release と MUSA の作業が終わるまでループ。判断事項は Genius で処理。人間の回答が要って無理なら別の作業を進める」

## 2. 判断の記録 (Genius 不在のため AI 判断)

Genius (判断カード DB) はこの拠点で稼働しておらず (127.0.0.1:4230 不通、clone 無し)、照会できなかった。
以下は AI が根拠付きで決めたもので、人間はいつでも覆せる。覆された項目は該当フェーズから作り直す。

| # | 論点 | 決定 | 根拠 |
|---|---|---|---|
| J1 | 「MUSA」の指す物 | **Ars-Musa の 3 アプリ (Melpomene / Terpsichore / Thaleia) の機能を Studio のプラグインとして C++ で実装する** | 指示が Studio の作り直しの文脈で出た。Ars-Musa は Tauri のデスクトップ道具で、中身はサービス層 (`src/services/*-api.ts`) と Rust バックエンドだけ。Ergo 側には MUSA の実装が `ergo_resource` (Clio の移植) 以外に無い = 「未実装」。LUDIARS/Thaleia (企画↔実装トレーサビリティの独立サービス) とは別物なので、プラグイン id は `musa_thaleia` とし混同を避ける |
| J2 | UI エンジン | `ergo_ui_kit` (ツリー・レイアウト・入力・アニメ) + Pictor `UIRenderer` (Vulkan 描画) + Pictor `text/` (文字のラスタライズ) | 既存の Ergo / Pictor の部品で完結し、第三者 UI ライブラリ (Dear ImGui 等) を持ち込まない。「C++ と UI エンジンのみ」に合う |
| J3 | 窓 | GLFW (Pictor が FetchContent で既に持つ) | Pictor のデスクトップ経路と同じ。新しい依存を増やさない |
| J4 | ゲーム側との通信 | Studio 内に loopback の HTTP/1.1 + WebSocket サーバを持ち、**既存のパスとポート (既定 5170、`/<id>/ws`、`/api/health`) を互換で保つ** | `ergo_bind` (`/variable/ws`)・`ergo_particle_link` (`/particle/ws`)・`ergo_scene_link` (`/scene_editor/ws`)・`ergo_log_live` (`/log/ws`) などの C++ クライアントを変えずに済む。ブラウザ UI は配信しない |
| J5 | プラグイン契約 | 組み込みプラグインは C++ の interface を ISP で分割 (`IPluginView` / `IPluginHttp` / `IPluginSocket` / `IPluginHealth`)。外部パックは C ABI の DLL (`ergo_studio_plugin_v1`) を `ERGO_PLUGIN_DIR` から読む | JS のパックはネイティブの process で読めない。C ABI にすればコンパイラの差に依存しない。ホストリポの既存 JS パックの移植はホストリポ側の作業で、この PR には入れない (名前依存を Ergo に持ち込まない) |
| J6 | プラグイン取得 (ST-INV-08) | 許可 owner・40 桁 sha の固定・プレビューした物をそのまま導入・台帳は維持。依存の導入は `npm install --ignore-scripts` から「同梱の CMake でビルドした DLL」に置き換える | 安全上の不変条件は言語に依存しない。ビルドは承認の後だけ行う |
| J7 | LLM 呼び出し (vfx-suggest / vfx-breed) | `claude -p` CLI をサブプロセスで呼ぶ (既存の既定 backend `cli` と同じ) | Anthropic SDK (TS) と Lapilli one-shot (TS) を外す。秘密を Studio が持たない |
| J8 | Rive Player | rive-runtime (C++、MIT) を FetchContent で取り込み、CPU で描いたフレームを `UIRenderer` のテクスチャとして出す | 他に C++ の Rive 再生手段が無い。取り込みに失敗したらそのプラグインだけ「未対応」と表示し、他を止めない (黙って別経路に切り替えない) |
| J9 | ブラウザ版 (`npm run serve`) | 廃止する | 「全部 C++」の指示。ブラウザ UI を残すと TS 版を二重に保守することになる |
| J10 | C++ の版 | C++17 (Ergo の基準に合わせる) | C++20 への移行は別 PR (2026-10-31 の残件) |
| J11 | Release | ネイティブ版が Excubitor 経由で起動し、全プラグインの health が ok になってから `revisor release` で作る。初期 version は `0.1.0` として、その後 `--kind minor` で `v0.2.0` を出す | ネイティブ化は機能追加 (minor)。Revisor 上の Ergo の version は未初期化なので、初期化が先に必要 |

## 3. 構成

```
include/ergo/studio/      … 公開 interface (plugin 契約 / host / net / view)
src/studio/
  net/                    … HTTP/1.1 パーサ、WebSocket (RFC 6455)、loopback listener、request guard
  host/                   … プラグインの登録・生成・重複 id 拒否・health 集約、外部 DLL の読み込み
  shell/                  … 窓 (GLFW)、Vulkan + UIRenderer、文字アトラス、タブ / ピッカー
  platform/               … プロセス実行 (git / revisor / claude)、OS の資格情報保管
tools/studio/
  main.cpp                … ergo_studio.exe の入口 (設定を読み、host と shell を組み立てるだけ)
  plugins/<id>/           … 組み込みプラグイン (1 プラグイン 1 ディレクトリ)
tests/studio/             … net / host / 各プラグインの純ロジックのテスト (mini-gtest)
```

- `ergo_ui_kit` に足す部品: TextField (IME の確定文字列・カーソル・選択)、ScrollView、List / Table (仮想化)、Tabs、キーボードフォーカスと Tab 移動。描画リストに Text を含めたまま、文字は shell の文字アトラスが描く。
- CMake: `ERGO_BUILD_STUDIO` (既定 OFF。Pictor + Vulkan + GLFW が揃うときだけ ON にできる)。
- Excubitor catalog: `runtime: app` のまま `app_kind: native`、`exec: ${ARS_ROOT}/Ergo/build/studio/ergo_studio.exe`、`build_command` は CMake の configure + build。

## 4. 移植対象 (全部)

| プラグイン | 移植元 (TS) | 主な中身 |
|---|---|---|
| studio | `tools/ergo/src/plugins/studio` | 6 タブ (プロジェクト / タスク / ソース / リソース / プラグイン / 連携)、ST-INV-01〜09 をそのまま守る |
| particle / variable / scene_editor / log | 同名 | ゲーム側からの WS の受け口 + 編集画面 |
| pictor_perf / profile / render_pipeline | 同名 | Pictor の計測・パイプラインの表示 |
| rive | 同名 | J8 |
| visus / ui_layout | 同名 | 編集画面 + 既存の HTTP 連携 |
| vfx-suggest / vfx-breed | 同名 | J7 |
| musa_melpomene | Ars-Musa `melpomene` | GitHub の Issue / コメント / PR / レビュー / Workflow run / Milestone、通知のポーリング、シーン・位置での絞り込み、キャッシュ。トークンは OS の資格情報保管 (Windows Credential Manager) |
| musa_terpsichore | Ars-Musa `terpsichore` | コマンドサーバの設定・状態・開始・停止、外部ツール連携 |
| musa_thaleia | Ars-Musa `thaleia` | Markdown のファイルツリー・読み込み・描画 (ui_kit のツリーへ変換)・カテゴリ・検索・スクリプトに対応する文書の検索 |

## 5. 進め方

1 PR に集約し (AI 実装は 1 PR)、コミットはフェーズ単位で積む。

1. ui_kit の部品と文字アトラス
2. net (HTTP / WS サーバ) と request guard
3. shell と plugin host と DLL の ABI
4. 既存 13 プラグインの移植
5. MUSA 3 プラグイン
6. Electron と TS 版の削除、catalog・仕様書・README・module_list の更新
7. ビルドとテスト → Revisor 提出 → マージ → Excubitor 経由の起動確認 (本体フォルダ) → Release

## 6. 影響

- ホストリポの JS プラグインパックは読み込まれなくなる。ホストリポごとに DLL パックへの移植が要る (各ホストの作業として Actio に登録する)。
- ブラウザから `http://127.0.0.1:5170/<id>/` を開く使い方は無くなる。
