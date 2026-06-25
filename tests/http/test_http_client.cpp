// Tests for ergo::http. Deterministic and offline: we never reach an external
// host. The transport is exercised by pointing it at a closed local port, which
// fails fast with "connection refused" — proving the real libcurl path runs and
// that failures are reported through Response rather than thrown.

#include "ergo/http/client.hpp"

#include "gtest/gtest.h"

namespace {

TEST(ErgoHttp, FactoryReturnsClient) {
    auto client = ergo::http::make_curl_client();
    ASSERT_NE(client, nullptr);
}

TEST(ErgoHttp, RefusedConnectionIsReportedNotThrown) {
    auto client = ergo::http::make_curl_client();
    ASSERT_NE(client, nullptr);

    // Port 1 on loopback is not listening: curl returns a couldn't-connect error
    // without any network egress.
    const ergo::http::Response r =
        client->get("http://127.0.0.1:1/", /*headers=*/{});

    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.status, 0);
    EXPECT_FALSE(r.error.empty());
    EXPECT_TRUE(r.body.empty());
}

TEST(ErgoHttp, UnsupportedSchemeFails) {
    auto client = ergo::http::make_curl_client();
    ASSERT_NE(client, nullptr);

    const ergo::http::Response r = client->get("not-a-real-scheme://nowhere");
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.error.empty());
}

TEST(ErgoHttp, ShortTimeoutIsReported) {
    auto client = ergo::http::make_curl_client();
    ASSERT_NE(client, nullptr);

    // Unroutable TEST-NET-1 address + 1ms timeout: the transfer cannot complete,
    // so we get ok == false with an error and no body — still no real traffic
    // that would succeed.
    ergo::http::Request req;
    req.url        = "http://192.0.2.1/";
    req.timeout_ms = 1;
    const ergo::http::Response r = client->send(req);

    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.error.empty());
}

} // namespace
