# Particle Effect Editor

ブラウザでパーティクルエフェクトを編集し、変更を WebSocket でリアルタイム
配信するエディタ。

```
[browser UI] ──WS── [particle-editor server] ──WS── [engine clients]
                       (truth, in memory)         (e.g. AdventureCube via ergo_particle)
```

サーバは編集状態の単一情報源。複数のブラウザを開いても全クライアントが
同期する。エンジン側 (AdventureCube 等) も同じ WS に接続して、受信した
config をそのまま自分のパーティクルシステムに流せばライブ編集が成立する。

## 場所

`ergo` リポジトリ ブランチ `module/particle` の `tools/particle-editor/`。
ergo_particle モジュール (C++) と同じブランチで管理されている。

## 起動

```bash
# standalone (ergo を直接 clone)
cd <ergo>/tools/particle-editor

# あるいは host 側 worktree 経由 (例: AdventureCube)
cd <host>/external/ergo/particle/tools/particle-editor

npm install
npm run dev    # tsx watch でホットリロード
# あるいは
npm start
```

デフォルト `http://localhost:5173/`。`PORT=NNNN npm start` で変更可。

## 構成

| パス | 内容 |
|---|---|
| `src/server.ts`  | Hono + ws の HTTP/WS サーバ。状態保持と broadcast |
| `src/schema.ts`  | `ParticleEffectConfig` 型・デフォルト値・wire protocol 定義 |
| `public/index.html` | 編集 UI (canvas プレビュー付き) |

## エンドポイント

### HTTP
- `GET  /` — エディタ UI
- `GET  /api/effect` — 現在の config を JSON で返す
- `POST /api/effect` — body の config をマージ適用 (deep merge) して broadcast
- `GET  /api/health` — `{ok, version, clients}`

### WebSocket (`/ws`)

JSON テキストフレーム。

サーバ → クライアント (接続時 / 状態変化時):
```json
{ "op": "state", "config": <ParticleEffectConfig>, "clients": 2 }
```

クライアント → サーバ:
```json
{ "op": "set",     "config": <Partial<ParticleEffectConfig>> }   // deep-merge
{ "op": "replace", "config": <ParticleEffectConfig> }            // full replace
{ "op": "ping" }
```

ブラウザ UI は UI 操作のたびに `set` を全 config 付きで送り (~30Hz throttle)、
受信した `state` で UI を再構築する。エンジン側は `state` を購読して
受信した config を CPU シミュレーションに食わせるだけで良い。

## 編集できるもの

| グループ | 主なパラメータ |
|---|---|
| Emission | rate (粒子/秒), maxAlive |
| Initial  | 発生半径, 速度角度+広がり, 速度min/max, 寿命min/max, サイズ, 色 |
| Over Life | サイズ補間 (start→end), 色補間 (start→end), 速度減衰 |
| Forces   | 重力 (vec2) |
| Render   | ブレンド (additive/alpha), 形状 (circle/square) |

座標系: Canvas に従い **+Y は下方向**。重力 `[0, 120]` で粒子は下へ落ちる。
速度角度も同様 (0°=+X, 90°=下, 270°=上)。

## プリセット

UI 内蔵: fountain / explosion / smoke / trail。

## クライアント実装ノート (エンジン側)

最小実装の要点:
1. 起動時に `ws://localhost:5173/ws` へ接続 (失敗時は再試行)
2. 受信ループで `op === "state"` を待ち、`config` を保存
3. 自分のパーティクルシステム (CPU sim) は保存した config を毎フレーム参照
4. 切断時はそれまでの config を保持しつつ再接続

ergo_particle (Ergo モジュール) と AdventureCube はこのパターンで統合する。

## 設計メモ
- ブラウザで開いた `file://` でも単独動作は可能 (WS は disabled、UI 内のローカル
  プレビュー canvas のみ動く)。サーバ越しに開いた `http://` では自動で WS 接続
- スキーマは `src/schema.ts` と `public/index.html` の `DEFAULTS`/`SCHEMA` 定数を
  両方更新する必要がある (将来的にビルド時生成にする余地あり)
