#pragma once

// 由来の HTTP アダプタの共通部品: loopback のサービスから JSON を 1 回 GET する (ergo_http)。
// リダイレクトは追わない (loopback の外へ出ない)。 失敗の理由に URL は入れない。

#include "ergo/http/client.hpp"

#include <string>

namespace ergo::package_cli {

struct HttpJson {
    bool        ok = false;
    std::string body;
    std::string reason;  ///< ok == false の理由 ("not reachable" / "HTTP 401" など)
};

HttpJson http_get_json(ergo::http::IHttpClient& client, const std::string& url);

} // namespace ergo::package_cli
