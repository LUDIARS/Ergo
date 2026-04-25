// HttpServer + HttpResponse helpers の単体テスト。
// 実 socket bind は test_custos_module で扱うので、ここは header-only ロジック中心。

#include "gtest/gtest.h"

// 直接 src/ の private header を見るので相対 include
#include "../../src/custos/http_server.h"

using namespace ergo::custos::detail;

TEST(HttpResponse, text_helper_sets_status_and_body) {
    auto r = HttpResponse::text(404, "nope");
    EXPECT_EQ(r.status, 404);
    EXPECT_EQ(r.content_type, "text/plain; charset=utf-8");
    ASSERT_EQ(r.body.size(), 4u);
    EXPECT_EQ(char(r.body[0]), 'n');
    EXPECT_EQ(char(r.body[3]), 'e');
}

TEST(HttpResponse, png_helper_keeps_bytes) {
    std::vector<std::uint8_t> bytes = { 0x89, 0x50, 0x4E, 0x47 };
    auto r = HttpResponse::png(std::vector<std::uint8_t>(bytes));
    EXPECT_EQ(r.status, 200);
    EXPECT_EQ(r.content_type, "image/png");
    EXPECT_EQ(r.body, bytes);
}

TEST(HttpResponse, json_sets_content_type) {
    auto r = HttpResponse::json(400, R"({"error":"x"})");
    EXPECT_EQ(r.status, 400);
    EXPECT_EQ(r.content_type, "application/json; charset=utf-8");
}

TEST(HttpResponse, not_found_and_not_implemented) {
    EXPECT_EQ(HttpResponse::not_found().status, 404);
    EXPECT_EQ(HttpResponse::not_implemented().status, 501);
}
