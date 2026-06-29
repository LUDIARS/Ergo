# pictor_perf — Pictor パフォーマンスモニタープラグイン

`tools/ergo` の built-in プラグイン。Pictor の `PerfQueryAPI::export_json()` が
吐く性能スナップショットを browser で可視化する。

- HTTP/UI: `/pictor_perf/`
- WS: `/pictor_perf/ws`
- 取り込み (headless host): `POST /pictor_perf/api/publish` (body = perf JSON)
- 単発取得: `GET /pictor_perf/api/snapshot`

## データ源

Pictor を回す host (ゲーム / Ergo render context) が `create_perf_query_api()` →
`export_json()` を定期的に publish する。経路は WS の `{op:"publish",snapshot}` か
HTTP POST の 2 つ。plugin は直近スナップショットを保持し接続 UI へ broadcast する。
`memory` フィールドを持たない payload は 400 で拒否する (無言で握り潰さない)。

## 可視化 (Pictor `spec/subsystem/perf_introspection.md`)

| セクション | 内容 |
|---|---|
| DoD 不変条件 | `float4x4==64` / `AABB==24` 等を chip 表示 |
| メモリレイアウト (A) | SoA stream 毎の要素サイズ / 先頭ptr アライメント / キャッシュライン跨ぎ / ライン利用率、アロケータ断片化 |
| キャッシュトラフィック (B) | パス毎の実効帯域 (GB/s) を bar 表示 |
| GPU バッチ適格性 | per-object→per-batch の畳み込み可否・理由 (mesh/material/透過/custom) |
| per-batch GPU 時間 | host が `BatchGpuTimer` で bracket した実測値 (未計測は明示) |
| HW カウンタ (C) | L2/L3 ヒット率・DRAM 帯域・IPC (Intel PCM)。未対応は理由付きで利用不可表示 |

## 依存

Pictor の `PerfQueryAPI` JSON 契約に追従する横断仕様。Pictor 側スキーマ変更時は
本プラグインの `app.js` 描画と同一 PR で更新すること。
