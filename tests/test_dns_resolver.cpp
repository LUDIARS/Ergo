#include <gtest/gtest.h>
#include "ergo/network/dns_resolver.h"

using namespace ergo::network;

class DnsResolverTest : public ::testing::Test {};

// localhost の解決
TEST_F(DnsResolverTest, ResolveLocalhost) {
    auto results = DnsResolver::resolve("localhost", 80);
    EXPECT_FALSE(results.empty());
    // localhost は 127.0.0.1 または ::1
    bool found_loopback = false;
    for (const auto& addr : results) {
        if (addr.ip == "127.0.0.1" || addr.ip == "::1") {
            found_loopback = true;
            break;
        }
    }
    EXPECT_TRUE(found_loopback);
}

// resolve_first で最初のアドレスが取得されること
TEST_F(DnsResolverTest, ResolveFirst) {
    ResolvedAddress addr;
    bool ok = DnsResolver::resolve_first("localhost", 8080, addr);
    EXPECT_TRUE(ok);
    EXPECT_EQ(addr.port, 8080);
    EXPECT_FALSE(addr.ip.empty());
}

// IP アドレスの直接解決
TEST_F(DnsResolverTest, ResolveIPAddress) {
    auto results = DnsResolver::resolve("127.0.0.1", 443);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results.front().ip, "127.0.0.1");
    EXPECT_EQ(results.front().port, 443);
}

// 存在しないホストの解決
TEST_F(DnsResolverTest, ResolveInvalidHost) {
    auto results = DnsResolver::resolve("this.host.definitely.does.not.exist.invalid", 80);
    EXPECT_TRUE(results.empty());
}

// resolve_first で存在しないホスト
TEST_F(DnsResolverTest, ResolveFirstInvalidHost) {
    ResolvedAddress addr;
    bool ok = DnsResolver::resolve_first("this.host.definitely.does.not.exist.invalid", 80, addr);
    EXPECT_FALSE(ok);
}
