#pragma once

#include "ergo/network/types.h"

namespace ergo::network {

/// DNS 解決クラス
/// ホスト名からIPアドレスへの変換を行う
class DnsResolver {
public:
    /// ホスト名を解決し、IPアドレスのリストを返す
    static std::vector<ResolvedAddress> resolve(const std::string& host, uint16_t port);

    /// ホスト名を解決し、最初のアドレスを返す
    static bool resolve_first(const std::string& host, uint16_t port, ResolvedAddress& out);
};

} // namespace ergo::network
