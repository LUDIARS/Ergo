// The entrance check of the two gates and the loopback-only operations
// (spec/feature/tool/ergo.md "口と入口"). The proxy headers' values are made
// here for each test: the check looks at their presence, never at a value.
#include "ergo/studio/ingress/entrance.h"
#include "ergo/studio/ingress/remote_scope.h"

#include <gtest/gtest.h>

#include <random>
#include <string>

using namespace ergo::studio;
using ingress::check_entrance;
using ingress::EntrancePolicy;

namespace {

/// A header value nobody wrote down: "<what>-<random number>".
std::string made_up(const char* what) {
    static std::mt19937_64 gen{std::random_device{}()};
    return std::string(what) + "-" + std::to_string(gen());
}

const char* kPublic  = "studio.example.com";
const char* kTailnet = "devbox.tail1234.ts.net";

EntrancePolicy policy() { return {"127.0.0.1", {kPublic}, {kTailnet}}; }

net::HttpRequest request(const std::string& host, const std::string& path = "/api/health") {
    net::HttpRequest r;
    r.method          = "GET";
    r.path            = path;
    r.target          = path;
    r.headers["host"] = host;
    return r;
}

bool refused(net::Gate gate, const net::HttpRequest& r, bool ws = false) {
    return check_entrance(policy(), gate, r, ws).problem.has_value();
}

} // namespace

TEST(StudioEntrance, TheLocalGateTakesLoopbackNamesWithASameOrigin) {
    EXPECT_FALSE(refused(net::Gate::local, request("127.0.0.1:5170")));
    EXPECT_FALSE(refused(net::Gate::local, request("localhost:5170")));
    auto same = request("127.0.0.1:5170");
    same.headers["origin"] = "http://127.0.0.1:5170";
    EXPECT_FALSE(refused(net::Gate::local, same));
    EXPECT_TRUE(check_entrance(policy(), net::Gate::local, same, false).who.empty());

    auto cross = request("127.0.0.1:5170");
    cross.headers["origin"] = "https://evil.example";
    EXPECT_TRUE(refused(net::Gate::local, cross));
    EXPECT_TRUE(refused(net::Gate::local, request("evil.example:5170")));
}

TEST(StudioEntrance, TheLocalGateRefusesPublishedNames) {
    auto pub = request(kPublic);
    pub.headers[ingress::kAccessAssertionHeader] = made_up("assertion");
    EXPECT_TRUE(refused(net::Gate::local, pub));   // the tunnel must point at the remote gate
    auto tail = request(kTailnet);
    tail.headers[ingress::kTailscaleLoginHeader] = made_up("login");
    EXPECT_TRUE(refused(net::Gate::local, tail));
    EXPECT_TRUE(refused(net::Gate::local, tail, true));   // WebSocket handshakes too
}

TEST(StudioEntrance, APublicNameNeedsTheAccessAssertion) {
    auto r = request(kPublic);
    EXPECT_TRUE(refused(net::Gate::remote, r));
    r.headers[ingress::kAccessAssertionHeader] = "";
    EXPECT_TRUE(refused(net::Gate::remote, r));   // empty = absent

    const std::string email = made_up("user") + "@example.com";
    r.headers[ingress::kAccessAssertionHeader] = made_up("assertion");
    r.headers[ingress::kAccessEmailHeader]     = email;
    const auto in = check_entrance(policy(), net::Gate::remote, r, false);
    EXPECT_FALSE(in.problem.has_value());
    EXPECT_EQ(in.who, email);
}

TEST(StudioEntrance, ATailnetNameNeedsTailscaleUserLogin) {
    auto r = request(kTailnet);
    EXPECT_TRUE(refused(net::Gate::remote, r));   // e.g. Funnel: from outside the tailnet
    EXPECT_TRUE(refused(net::Gate::remote, r, true));   // a handshake without Origin is not let through

    const std::string login = made_up("someone") + "@github";
    r.headers[ingress::kTailscaleLoginHeader] = login;
    const auto in = check_entrance(policy(), net::Gate::remote, r, false);
    EXPECT_FALSE(in.problem.has_value());
    EXPECT_EQ(in.who, login);
}

TEST(StudioEntrance, TheRemoteGateRefusesOtherHostsAndCrossOrigins) {
    auto loop = request("127.0.0.1:5171");
    EXPECT_TRUE(refused(net::Gate::remote, loop));   // neither a public nor a tailnet name
    auto other = request("other.example.com");
    other.headers[ingress::kAccessAssertionHeader] = made_up("assertion");
    EXPECT_TRUE(refused(net::Gate::remote, other));

    auto cross = request(kTailnet);
    cross.headers[ingress::kTailscaleLoginHeader] = made_up("login");
    cross.headers["origin"]                       = "https://evil.example";
    EXPECT_TRUE(refused(net::Gate::remote, cross));
    cross.headers["origin"] = std::string("https://") + kTailnet;
    EXPECT_FALSE(refused(net::Gate::remote, cross));
}

TEST(StudioEntrance, TailnetNamesAreTheTsNetOnes) {
    const auto names = ingress::parse_tailnet_hosts(" DevBox.Tail1234.ts.net , studio.example.com,,x.ts.net:8443");
    ASSERT_EQ(names.size(), 1u);
    EXPECT_EQ(names[0], "devbox.tail1234.ts.net");
}

TEST(StudioRemoteScope, LoopbackOnlyOperationsAreRefusedAndReadingIsNot) {
    using ingress::remote_refusal;
    EXPECT_TRUE(remote_refusal("POST", "/api/launcher/add").has_value());
    EXPECT_TRUE(remote_refusal("POST", "/api/launcher/remove").has_value());
    EXPECT_TRUE(remote_refusal("POST", "/studio/api/plugins/install").has_value());
    EXPECT_TRUE(remote_refusal("DELETE", "/studio/api/plugins/x").has_value());
    EXPECT_TRUE(remote_refusal("POST", "/lesson/api/link").has_value());
    EXPECT_TRUE(remote_refusal("POST", "/git/api/commit").has_value());

    EXPECT_FALSE(remote_refusal("GET", "/studio/api/plugins").has_value());
    EXPECT_FALSE(remote_refusal("POST", "/api/launcher/open").has_value());
    EXPECT_FALSE(remote_refusal("POST", "/studios/api/x").has_value());   // another plugin's prefix
    EXPECT_FALSE(remote_refusal("POST", "/tuning/api/set").has_value());
}
