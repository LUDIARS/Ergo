# HTTP モジュール定義

## 概要

ホストアプリ共通の **同期 HTTP クライアント**。「この URL を取ってくる / POST
する」程度の用途のための最小インターフェイス。各モジュールが個別に libcurl を
叩いたり、OS 別の HTTP API を書き分けたりするのを防ぐ。

公開 API は `std::` 型のみで、**libcurl の型は一切露出しない** (consumer は
`ergo_http` をリンクするだけで `<curl/curl.h>` を見ない)。実装は libcurl
バックエンド (`make_curl_client()`) だが、呼び出し側は `IHttpClient` 抽象に
依存するため、transport の差し替え / フェイク化ができる (DIP)。

スコープ外: ストリーミング / chunked 受信、非同期、コネクションプール、cookie、
multipart。これらが要るモジュールは本モジュールの上 (または横) に積む。

## カテゴリ
システム

## 所属ドメイン
ランタイム基盤 / ネットワーク取得

## 必要なデータ
- リクエスト: URL (`std::string`)、メソッド (`Method::Get`/`Post`)、body、
  Content-Type、追加ヘッダ、timeout、リダイレクト追従可否 (`Request`)
- レスポンス: 成否 (`ok`)、HTTP ステータス、body、ヘッダ、transport エラー文字列
  (`Response`)

## 依存
- 管理対象サードパーティ **libcurl** (`CURL::libcurl`、PRIVATE リンク)。
  → `ERGO_BUILD_HTTP=ON` は `ERGO_WITH_CURL=ON` を強制する
  (取得不可なら FATAL_ERROR、無言フォールバックしない)。
- (テスト) mini-gtest
- 既定 **OFF**。libcurl が取得/ビルドを伴う opt-in 依存のため、素の Ergo ビルドを
  ネットワーク非依存・軽量に保つ目的で OFF にしている。

## 変数
- なし (クライアントは easy ハンドルを send 毎に作る。状態を持たない)

## 作業

### 入力
- `make_curl_client()` → `std::unique_ptr<IHttpClient>`
- `client->get(url, headers={})`
- `client->post(url, body, content_type="application/octet-stream", headers={})`
- `client->send(Request)` — 上記の素のプリミティブ (唯一の virtual)

### 出力
- `Response{ ok, status, body, headers, error }` を返す
- **例外を投げない**。失敗は `ok == false` + `error` に集約する
- `ok == true` は「転送が完了した」を意味する (HTTP 4xx/5xx でも true、
  status を見て判断する)。接続失敗・タイムアウト・不正 URL は `ok == false`

### タスク
- `curl_global_init` はプロセス内で一度だけ (function-local static の RAII
  `ensure_curl_global_init()`、`curl_global_cleanup` は静的破棄時)
- ヘッダ収集は header callback で "Name: Value" 行のみ採取 (status 行・空行は除外)
- POST は `CURLOPT_POST` + `POSTFIELDS`/`POSTFIELDSIZE`、Content-Type と追加
  ヘッダは `curl_slist` で付与
- `CURLOPT_NOSIGNAL=1` (マルチスレッド安全なタイムアウト)

## ファイル構成 (SRP)

| ファイル | 責務 |
|---|---|
| `include/ergo/http/client.hpp` | 公開インターフェイス (`IHttpClient` / `Request` / `Response` / `Method` / `make_curl_client`) |
| `src/http/curl_client.cpp` | libcurl 実装 (`CurlClient`) |
| `src/http/curl_global.hpp` · `curl_global.cpp` | プロセス一度きりの `curl_global_init` RAII |

## テスト

オフライン・決定的 (外部ホストに到達しない):
- `make_curl_client()` が非 null
- 閉じたローカルポート (`127.0.0.1:1`) への GET は `ok == false` / status 0 /
  error 非空 / body 空 (= 実 libcurl パスが走り、失敗が例外でなく Response で返る)
- 不正スキームの URL は `ok == false`
- 到達不能アドレス + 1ms timeout は `ok == false` + error 非空
