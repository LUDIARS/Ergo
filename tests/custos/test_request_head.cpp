// ergo_custos のリクエストヘッダ検査 (request_head.h) の境界テスト。
// 上限ちょうど / +1 / 壊れた入力 / 巨大な宣言を、ソケットなしで確かめる。

#include "gtest/gtest.h"

#include "../../src/custos/request_head.h"

#include <string>

using namespace ergo::custos::detail;

namespace {

/// "POST /key HTTP/1.0\r\n" + extra + "\r\n" を、終端込みで total バイトにそろえる。
std::string head_of_size(std::size_t total) {
    std::string head = "GET /health HTTP/1.0\r\nX-Pad: ";
    const std::string tail = "\r\n\r\n";
    head.append(total - head.size() - tail.size(), 'a');
    head += tail;
    return head;
}

HeadStatus parse(const std::string& head, std::size_t& len, const RequestLimits& limits = {}) {
    HttpRequest req;
    return parse_request_head(head, limits, req, len);
}

} // namespace

TEST(CustosRequestHead, parses_method_path_and_length) {
    HttpRequest req;
    std::size_t len = 99;
    const auto st = parse_request_head("POST /key HTTP/1.0\r\ncontent-length: 23\r\n\r\n", {}, req, len);
    EXPECT_EQ(static_cast<int>(st), 200);
    EXPECT_EQ(req.method, "POST");
    EXPECT_EQ(req.path, "/key");
    EXPECT_EQ(len, 23u);
}

TEST(CustosRequestHead, head_exactly_at_limit_is_accepted) {
    const RequestLimits limits;
    std::size_t len = 0;
    EXPECT_EQ(static_cast<int>(parse(head_of_size(limits.max_head), len)), 200);
}

TEST(CustosRequestHead, head_one_over_limit_is_431) {
    const RequestLimits limits;
    std::size_t len = 0;
    EXPECT_EQ(static_cast<int>(parse(head_of_size(limits.max_head + 1), len)), 431);
}

TEST(CustosRequestHead, unterminated_head_at_limit_is_431) {
    const RequestLimits limits;
    std::size_t len = 0;
    std::string head = "GET /health HTTP/1.0\r\nX-Pad: ";
    head.append(limits.max_head - head.size(), 'a');
    EXPECT_EQ(static_cast<int>(parse(head, len)), 431);
    EXPECT_EQ(static_cast<int>(parse("GET / HTTP/1.0\r\n", len)), 400);
}

TEST(CustosRequestHead, body_exactly_at_limit_and_one_over) {
    const RequestLimits limits;
    std::size_t len = 0;
    const std::string at = "POST /key HTTP/1.0\r\nContent-Length: " + std::to_string(limits.max_body) + "\r\n\r\n";
    EXPECT_EQ(static_cast<int>(parse(at, len)), 200);
    EXPECT_EQ(len, limits.max_body);
    const std::string over = "POST /key HTTP/1.0\r\nContent-Length: " + std::to_string(limits.max_body + 1) + "\r\n\r\n";
    EXPECT_EQ(static_cast<int>(parse(over, len)), 413);
}

TEST(CustosRequestHead, huge_declared_lengths_are_refused) {
    std::size_t len = 0;
    // size_t に収まる巨大値は 413、収まらない桁数は形の不正 (400)。
    EXPECT_EQ(static_cast<int>(parse("POST /key HTTP/1.0\r\nContent-Length: 4294967296\r\n\r\n", len)), 413);
    EXPECT_EQ(static_cast<int>(parse("POST /key HTTP/1.0\r\nContent-Length: 18446744073709551615\r\n\r\n", len)), 413);
    EXPECT_EQ(static_cast<int>(parse("POST /key HTTP/1.0\r\nContent-Length: 18446744073709551616\r\n\r\n", len)), 400);
}

TEST(CustosRequestHead, malformed_lengths_are_400) {
    std::size_t len = 0;
    for (const std::string v : {"-1", "+5", "1 2", "0x10", "", "12a"}) {
        EXPECT_EQ(static_cast<int>(parse("POST /key HTTP/1.0\r\nContent-Length: " + v + "\r\n\r\n", len)), 400);
    }
    // 食い違う 2 つの Content-Length (要求の密輸) は 400、同じ値の重複は通す。
    EXPECT_EQ(static_cast<int>(parse("POST /k HTTP/1.0\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\n", len)), 400);
    EXPECT_EQ(static_cast<int>(parse("POST /k HTTP/1.0\r\nContent-Length: 2\r\nContent-Length: 2\r\n\r\n", len)), 200);
}

TEST(CustosRequestHead, malformed_request_lines_are_400) {
    std::size_t len = 0;
    for (const std::string h : {"\r\n\r\n", "GET\r\n\r\n", "GET /\r\n\r\n", " / HTTP/1.0\r\n\r\n",
                                "GET  HTTP/1.0\r\n\r\n", "GET / HTTP/1.0\r\nno-colon\r\n\r\n",
                                "GET / HTTP/1.0\r\n: empty-name\r\n\r\n"}) {
        EXPECT_EQ(static_cast<int>(parse(h, len)), 400);
    }
}
