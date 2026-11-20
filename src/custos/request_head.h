#pragma once
//
// ergo_custos の HTTP/1.0 リクエストヘッダの検査 (ソケットに触らない純粋関数)。
// http_server.cpp の受信ループはここで長さと形を確かめてから本文を読む。
//
// 上限 (Custos backend が送るのは GET /health・GET /screenshot・POST /key の
// 数十バイトの JSON だけ):
//   - ヘッダ (リクエスト行 + ヘッダ行 + 空行) は 16 KiB まで。旧実装の MAX_HEADER と同じ値。
//   - 本文 (Content-Length) は 64 KiB まで。/key の本文 ({"code":87,"down":true}) の
//     千倍以上の余裕。旧実装は Content-Length をそのまま信じて、宣言された長さまで
//     受信し続けていた (上限なし)。

#include "http_server.h"

#include <cstddef>
#include <string>

namespace ergo::custos::detail {

struct RequestLimits {
    std::size_t max_head = 16 * 1024;
    std::size_t max_body = 64 * 1024;
};

/// 検査の結果。Ok 以外はそのまま応答のステータスになる。
enum class HeadStatus : int {
    Ok              = 200,
    Malformed       = 400,  // リクエスト行・ヘッダ行・Content-Length の形が不正
    BodyTooLarge    = 413,  // Content-Length が max_body を超える
    HeadTooLarge    = 431,  // 終端 (\r\n\r\n) が max_head までに無い
};

/// `head` は受信済みのバイト列の先頭から、終端の "\r\n\r\n" を含むまで。
/// method / path を `out` に、本文の長さを `content_length` に入れる。
/// Content-Length は 10 進の数字だけ (符号・空白混じり・空は Malformed)。
/// 同じ名前のヘッダが複数あって値が食い違う場合も Malformed。
HeadStatus parse_request_head(const std::string& head, const RequestLimits& limits,
                              HttpRequest& out, std::size_t& content_length);

} // namespace ergo::custos::detail
