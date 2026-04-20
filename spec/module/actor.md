# Actor モジュール定義

## 概要

シーンオブジェクトの **ツリー構造** と **変数公開** を一体で提供するバイ
ンディング基底モジュール。ホスト側では `ergo::actor::Actor` を継承する
ことで:

1. ツリーノードとして自動登録される (name / parent / handle)
2. `bind_var()` / `bind_accessor()` 経由で公開した変数が **そのアクター
   配下に紐付く** 形で variable プラグインに配信される

UI (`tools/ergo/` の variable プラグイン) 側では Actor ツリーを左ペイン、
選択した Actor の変数を右ペインに表示する。

## カテゴリ
システム

## 所属ドメイン
シーングラフ / 開発者ツール / ライブチューニング

## 必要なデータ
- Actor インスタンスのメタ情報: `handle` (1 から始まる uint64), `name`, `parent_handle`
- そのアクターが bind した変数の一覧 (ergo_bind が保持)
- ホストとツール間の wire protocol — `actor_register` / `actor_unregister`
  + `bind` の meta に `actor_handle`

## 依存
- `ergo_bind` (必須) — 変数公開の実装を委譲、プロトコル拡張のキャリア
- C++17 標準ライブラリ
- (テスト) mini-gtest

## 変数
- グローバル Registry (プロセス内単調増加ハンドル発行、親子リンク、
  Actor 追加・削除時のブロードキャスト)
- Actor インスタンス毎: name, handle, parent, children[], owned_binds[]

## 作業

### 入力
- `Actor(name, parent=nullptr)` 構築 → `actor_register(handle, parent_handle, name)` を Engine に送る
- `~Actor()` 破棄 → `actor_unregister(handle)` を送る
- `bind_var<T>(name, ptr, meta)` / `bind_accessor(name, kind, get, set, meta)` を protected で提供
  - 既定で `meta.actor_handle = this->handle()` を詰めてから `ergo::bind::Engine::instance().bind(...)` に委譲
  - 変数 name は既定で `"{actor_name}.{var_name}"` に qualify

### 出力
- WebSocket wire に新たな 2 種のメッセージ:
  - `{ op: "actor_register",   handle, parent, name }`
  - `{ op: "actor_unregister", handle }`
- 既存の bind メッセージに `meta.actor_handle` フィールドを追加 (0 = グローバル)

### タスク
- プロセス内ハンドル発行 (atomic counter)
- Registry が Actor* を持つ (弱参照ではなく所有ポインタは持たない。Actor の ctor/dtor で直接 add/remove)
- Engine への (re)connect 時、Registry のスナップショットを `actor_register` で再送
- ダミープラグ (リンクのみ満たす no-op)

## テスト

- Actor 単体: handle が 0 でない、親子リンクが正しく張られる
- 2 個生成で handle がユニーク
- `bind_var` で Engine に届く meta の actor_handle が自身の handle と一致
- 親 Actor → 子 Actor の順で作っても / 逆でも children が一貫

## UI 側 (tools/ergo variable プラグイン)

- 左ペイン: Actor ツリー (折りたたみ + 選択)
- 右ペイン: 選択中 Actor の変数一覧 (flat もとのビュー)
- actor_handle=0 (グローバル) の変数は仮想 root "(global)" 配下に表示
- 選択していない状態では全変数を表示 (既存挙動互換)

## 制約 / 先送り

- 親子の動的な付け替え (reparent) は初版スコープ外
- Actor 側のフィールド (visible / enabled / transform) は次フェーズ
- 編集側 (tree 操作で Actor を disable にする等) は初版スコープ外 (read-only)
