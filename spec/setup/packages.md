# LUDIARS パッケージ・ハブ (ホスト向け手順)

Ergo が Pictor / Tela などの LUDIARS 製 C++ パッケージの版を 1 か所で固定し、
何がリンクされているかを記録・可視化する仕組み。設計の正本は
[`spec/feature/package-hub.md`](../feature/package-hub.md)、マニフェストの形式は
[`spec/data/ergo-packages.md`](../data/ergo-packages.md)。

- 対象: ホストにリンクされる LUDIARS 製 C++ パッケージ (link)、取得だけするもの (tool: Lapilli)、
  読むだけの関連リポ (observe: MUSA 系・Curare)
- 対象外: 第三者ライブラリ (kazmath / curl など) は従来どおり
  [`third-party-deps.md`](third-party-deps.md) の仕組み

## 必要なもの

- CMake **3.20 以上** (Ergo の `cmake_minimum_required` を 3.16 から 3.20 に上げた。Pictor / Tela も 3.20)。
  3.20 未満の CMake で `add_subdirectory(<ergo>)` すると構成エラーになる

## 今の段階 (S1: 記録のみ)

| モード | ホストの状態 | Ergo の動き | 段階 |
|---|---|---|---|
| **M0 互換 (既定)** | `ERGO_PACKAGES` 未設定。今までどおりホストが Pictor などを取り込む | 取り込まない。既にあるターゲットを `host-unmanaged` として記録するだけ。**ビルド結果は変わらない** (ターゲット集合と compile definitions が同じ) | S1 (今) |
| M1 照合 | `ERGO_PACKAGES` を設定、取り込みはまだホスト | ホスト供給経路で受け、ターゲットの `SOURCE_DIR` の git HEAD を pin と照合する (PKG-05) | S2 (予定) |
| M2 委譲 | ホストが自前の取り込み・SHA 検証・`PICTOR_BUILD_*` の FORCE を消す | ハブがローカルソース / 兄弟チェックアウト / 取得で取り込む | S2 (予定) |

S1 で `ERGO_PACKAGES` を設定した場合は、選択の検査 (PKG-01 / 02 / 06 / 11 / 12 / 13 / 14 / 15)
だけを行い、error があれば全件を並べて構成を止める。通った場合も
「S1 は検査と記録だけ」という WARNING を出し、**取り込みは行わない** (ホストが引き続きターゲットを作る)。

## M0 で何もしなくてよいこと

既存ホストは変更不要。Ergo を `add_subdirectory` するだけで、構成の最後に次が書かれる。

- `<build>/ergo-packages.resolved.json`: パッケージごとの pin・状態 (`host-unmanaged` / `module-found` / `absent`)・
  既定で選ばれる機能、モジュールごとの ON/OFF と理由 (`ERGO_RENDER_BACKEND_REASON` など)、検査結果
- `<build>/.cmake/api/v1/query/client-ergo/query.json`: CMake File API (codemodel v2) の問い合わせ

構成ログには 1 行の要約が出る。

```
-- ergo packages: mode=M0 (pictor=host-unmanaged, tela=absent), 0 finding(s) -> <build>/ergo-packages.resolved.json
```

M0 では検査結果は記録だけで、構成を止めない (PKG-14 も `record`。実描画経路の可否は
従来どおり `ergo_render_enforce_backend` が決める)。

## 変数 (すべて `add_subdirectory(<ergo>)` より前のキャッシュ変数)

| 変数 | S1 での扱い | 意味 |
|---|---|---|
| `ERGO_PACKAGES` | 有効 | 使う link パッケージの列 (例 `pictor;tela`)。**未設定なら M0** |
| `ERGO_PACKAGE_FEATURES_<NAME>` | 有効 (検査と記録) | 機能の足し引き (`+web;-vulkan`、`web` は `+web` と同じ)。不明な名前・予約機能は PKG-13 |
| `ERGO_PACKAGE_REV_<NAME>` | 有効 (検査と記録) | pin の一時上書き (検証用)。PKG-06 の警告、STRICT では error。値も 40 桁 SHA であること |
| `ERGO_PACKAGES_STRICT` | 有効 | 警告扱いを error にする (CI と出荷構成で ON) |
| `ERGO_PACKAGE_SOURCE_DIR_<NAME>` | S2 | ローカルソースを明示 (worktree・オフライン) |
| `ERGO_PACKAGES_SIBLING_ROOT` | S2 | 兄弟チェックアウト探索の明示 opt-in (`<root>/<Repo>` だけ。`.worktrees` は見ない) |
| `ERGO_PACKAGE_PROVIDED_<NAME>` | S2 | ホストが自分で作ると明示 (照合は行う) |
| `ERGO_PACKAGES_FETCH` | S2 | 取得を許すか。既定 OFF |
| `ERGO_PACKAGE_MANIFEST_EXTRA` | S2 | ホスト側の追加マニフェスト (非公開パッケージ用)。同名は PKG-15 の error |

`<NAME>` はパッケージ名を大文字にし `-` を `_` にしたもの (`pictor` → `PICTOR`)。

## 今後の移行 (S2 以降の予告)

- 兄弟ディレクトリを `add_subdirectory` するホスト: M1 で `ERGO_PACKAGES=pictor` を足して照合 →
  M2 で `add_subdirectory(../Pictor)` を消し `ERGO_PACKAGE_SOURCE_DIR_PICTOR` を渡す。
- Pictor と Ergo を別々の SHA で FetchContent し自前で検証するホスト: M1 で照合して食い違いを
  どちらに寄せるか決める → M2 で Ergo の SHA だけを固定し、`ERGO_PACKAGES_FETCH=ON` + `ERGO_PACKAGES_STRICT=ON`。
  Pictor より前に走る必要のあるモバイルの Vulkan 結線はホストに残す。
- Ergo の一部モジュールだけを使うホスト: M0 のままでよい。
- Pictor の新しい SHA を使いたいときは、Ergo の `ergo-packages.json` の `rev` を上げる PR を出す。

## 既知の制約

- Pictor は desktop で glfw を暗黙に FetchContent する。ハブでは直せないため、オフラインで完結する
  構成は保証できない (Pictor 側に opt-out を依頼する。統合設計 §8 / N6)。
- Windows のパス長: 取得 (S2) では `ERGO_DEPENDENCY_CACHE_DIR` を短いパスに置く。

## 検証

| 何を | どうやって |
|---|---|
| 規則 (PKG 各項目の真偽・循環・機能の足し引き・resolved.json の決定性) | `ctest -R test_packages` (`tests/packages/test_*.cmake`、`cmake -P`。コンパイラ不要) |
| M0 でターゲット集合と compile definitions が変わらない (偽ホスト) | `ctest -R test_packages_m0_invariance` (`tests/packages/fixtures/m0_host`) |
| 同じこと (実ツリー全体、基準チェックアウトとの比較) | `cmake -DBASE_SOURCE=<基準> -DNEW_SOURCE=<新> -DWORK_DIR=<作業> [-DGENERATOR=...] -P tests/packages/compare_m0_build.cmake` |
