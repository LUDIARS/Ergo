# IO モジュール定義

## 概要

ホストアプリ共通の **ファイル I/O ラッパー**。
セーブ/ロード、設定ファイル、アセットの読み書きで各モジュールが個別に
`std::ifstream` / `std::ofstream` を書き直すことを防ぐための、最小限の
共通インターフェイス。

C++17 `<filesystem>` を薄くラップしているだけなので依存は標準ライブラリ
のみ。JSON パースや高レベルなシリアライズはスコープ外 — それは呼び出し
側の仕事。本モジュールは純粋にバイト列を読み書きする。

## カテゴリ
システム

## 所属ドメイン
ランタイム基盤 / セーブ & ロード

## 必要なデータ
- ファイルパス (`std::string`, UTF-8 想定)
- ファイルの中身 (`std::string`)

## 依存
- C++17 標準ライブラリ (`<filesystem>`, `<fstream>`)
- (テスト) mini-gtest

## 変数
- なし (純粋関数のみ)

## 作業

### 入力
- 読み出し: `read_file(path, out_content)` — バイナリ I/O でファイル全体を読む
- 書き込み: `write_file(path, content)` — バイナリで全上書き。親ディレクトリは自動作成
- 存在確認: `exists(path)` / `is_directory(path)`
- ディレクトリ作成: `ensure_directory(path)`
- 削除: `remove_file(path)`

### 出力
- 成功/失敗を bool で返す
- 例外は内部で握り潰して bool に変換 (`<filesystem>` が投げる `filesystem_error` など)

### タスク
- 読み書きはバイナリモード固定 (CRLF 変換なし)
- 書き込みは親ディレクトリを `ensure_directory` してから行う
- ダミープラグ (no-op) を提供

## テスト

- round-trip: 書いた内容が同一バイト列で読み戻せる
- 新規ディレクトリ + 新規ファイルの同時作成
- 存在しないファイルへの `read_file` は false
- `is_directory` が file / dir を正しく識別
- `remove_file` の後 `exists` は false

## Path ヘルパ (補助)

`path.h` に少量のユーティリティ:
- `parent_of(path)` — 親ディレクトリ
- `join(a, b)` — `/` または `\\` のどちらでも受け取り、OS 区切りで結合
- `extension_of(path)` / `stem_of(path)`

これらは `std::filesystem::path` の薄いラッパ (将来 Win32 API に置換
できるよう、公開 API は `std::string` のみを使う)。
