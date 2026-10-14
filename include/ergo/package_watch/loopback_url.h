#pragma once

// Revisor / Concordia の URL の検査 (spec/feature/package-manager.md §3)。 ergo_pkg watch が読むのは同じ機械の
// サービスだけで、 loopback 以外の URL は引数の誤り (終了 2)。 ポートの数字は Ergo に書かない (ジョブが渡す)。

#include <string>

namespace ergo::package_watch {

/// http(s)://<host>[:<port>][/...] で、 host が localhost / 127.0.0.0/8 / [::1]。 利用者情報 (user@) は拒む。
bool is_loopback_url(const std::string& url);

} // namespace ergo::package_watch
