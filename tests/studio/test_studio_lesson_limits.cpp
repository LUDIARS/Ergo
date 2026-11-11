// Upper bounds of what the lesson plugin takes from outside: the pasted
// connection string and odeum-lesson's replies (lesson_link.h,
// lesson_protocol.h kMax*). Each limit: exactly the limit passes, one past it is
// refused with an explicit error, broken and huge declared input is refused.
// spec/setup/hardening.md "入力の上限".
#include "fake_http.h"
#include "plugins/lesson/lesson_client.h"
#include "plugins/lesson/lesson_link.h"
#include "plugins/lesson/lesson_protocol.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

using namespace ergo::studio::plugins::lesson;
using namespace studio_test;

namespace {

const char* const kOrigin = "http://127.0.0.1:4410";
const char* const kToken  = "0123456789abcdef0123456789abcdef";

std::string quoted(const std::string& s) { return "\"" + s + "\""; }

/// A student state with `steps` steps, `features` features and the given title.
std::string student_state(std::size_t steps, std::size_t features, const std::string& title = "t",
                          const std::string& label = "l") {
    std::string f;
    for (std::size_t i = 0; i < features; ++i) {
        if (i) f += ",";
        f += quoted("f" + std::to_string(i)) + ":" + quoted("v" + std::to_string(i));
    }
    std::string s;
    for (std::size_t i = 0; i < steps; ++i) {
        if (i) s += ",";
        s += R"({"id":")" + std::to_string(i) + R"(","label":)" + quoted(label) + R"(,"done":false})";
    }
    return R"({"role":"student","class":{"title":)" + quoted(title) + R"(},"features":{)" + f +
           R"(},"assignment":{"id":"a","title":"t","active":true,"steps":[)" + s + "]}}";
}

std::string presenter_state(std::size_t entries) {
    std::string st;
    for (std::size_t i = 0; i < entries; ++i) {
        if (i) st += ",";
        st += quoted("f" + std::to_string(i)) + ":true";
    }
    return R"({"role":"presenter","class":{"title":"t"},"features":{},"assignment":null,"demo":{"seq":1,"states":{)" +
           st + "}}}";
}

std::string pair_reply(const std::string& title) {
    return std::string(R"({"token":")") + kToken + R"(","role":"student","class":{"title":)" + quoted(title) +
           R"(},"poll_ms":2000})";
}

} // namespace

TEST(StudioLessonLimits, LinkTextUpTo2048Bytes) {
    const std::string link = "http://127.0.0.1:4410/v1/tool#7K2M9QX4TB";
    // Copied with surrounding white space: the limit counts the raw text.
    const std::string at_limit = std::string(kMaxLinkText - link.size(), ' ') + link;
    ASSERT_EQ(at_limit.size(), kMaxLinkText);
    EXPECT_TRUE(parse_lesson_link(at_limit).link.has_value());
    const auto over = parse_lesson_link(" " + at_limit);
    EXPECT_FALSE(over.link.has_value());
    EXPECT_FALSE(over.error.empty());
    // A huge paste is refused before it is copied or scanned.
    const auto huge = parse_lesson_link(std::string(32u * 1024u * 1024u, 'a'));
    EXPECT_FALSE(huge.link.has_value());
}

TEST(StudioLessonLimits, ReplyBodyUpTo1MiB) {
    const std::string base = student_state(1, 0);
    // Pad with white space after the object: valid JSON of exactly the limit.
    const std::string at_limit = base + std::string(kMaxReplyBytes - base.size(), ' ');
    ASSERT_EQ(at_limit.size(), kMaxReplyBytes);
    EXPECT_TRUE(parse_state(at_limit).value.has_value());
    const auto over = parse_state(at_limit + " ");
    EXPECT_FALSE(over.value.has_value());
    EXPECT_FALSE(over.error.empty());
    EXPECT_FALSE(parse_pair_reply(pair_reply("t") + std::string(kMaxReplyBytes, ' ')).value.has_value());
    EXPECT_FALSE(parse_progress_ack(std::string(R"({"ok":true})") + std::string(kMaxReplyBytes, ' ')).empty());
}

TEST(StudioLessonLimits, StepsAndFeaturesUpTo256) {
    EXPECT_TRUE(parse_state(student_state(kMaxSteps, 0)).value.has_value());
    EXPECT_FALSE(parse_state(student_state(kMaxSteps + 1, 0)).value.has_value());
    EXPECT_TRUE(parse_state(student_state(1, kMaxFeatures)).value.has_value());
    EXPECT_FALSE(parse_state(student_state(1, kMaxFeatures + 1)).value.has_value());
    EXPECT_TRUE(parse_state(presenter_state(kMaxFeatures)).value.has_value());
    EXPECT_FALSE(parse_state(presenter_state(kMaxFeatures + 1)).value.has_value());
    // All three at their limit together still fit one reply.
    EXPECT_TRUE(parse_state(student_state(kMaxSteps, kMaxFeatures)).value.has_value());
}

TEST(StudioLessonLimits, StringsUpTo4096Bytes) {
    const std::string at(kMaxText, 'x');
    const std::string over(kMaxText + 1, 'x');
    EXPECT_TRUE(parse_state(student_state(1, 0, at)).value.has_value());
    EXPECT_FALSE(parse_state(student_state(1, 0, over)).value.has_value());
    EXPECT_TRUE(parse_state(student_state(1, 0, "t", at)).value.has_value());
    EXPECT_FALSE(parse_state(student_state(1, 0, "t", over)).value.has_value());
    EXPECT_TRUE(parse_pair_reply(pair_reply(at)).value.has_value());
    EXPECT_FALSE(parse_pair_reply(pair_reply(over)).value.has_value());
}

TEST(StudioLessonLimits, AllKeptStringsUpTo256KiB) {
    // 64 labels of 4096 bytes = the 256 KiB budget; the title and ids push it over.
    const std::string label(kMaxText, 'x');
    const std::size_t fit = kMaxReplyTextSum / kMaxText - 1;   // leaves room for the other strings
    EXPECT_TRUE(parse_state(student_state(fit, 0, "t", label)).value.has_value());
    const auto r = parse_state(student_state(fit + 2, 0, "t", label));
    EXPECT_FALSE(r.value.has_value());
    EXPECT_FALSE(r.error.empty());
}

TEST(StudioLessonLimits, DeepOrValueHeavyRepliesAreRefused) {
    // Nesting past the reply depth, in a field the parser would otherwise skip.
    std::string deep = R"({"role":"student","class":{"title":"t"},"features":{},"assignment":null,"extra":)";
    deep += std::string(kMaxReplyDepth + 1, '[') + std::string(kMaxReplyDepth + 1, ']') + "}";
    EXPECT_FALSE(parse_state(deep).value.has_value());
    // More JSON values than the reply allows, also in an ignored field.
    std::string many = R"({"role":"student","class":{"title":"t"},"features":{},"assignment":null,"extra":[)";
    for (std::size_t i = 0; i < kMaxReplyValues; ++i) many += i ? ",0" : "0";
    many += "]}";
    EXPECT_FALSE(parse_state(many).value.has_value());
    // Broken input stays an explicit error.
    EXPECT_FALSE(parse_state(student_state(3, 3).substr(0, 40)).value.has_value());
}

TEST(StudioLessonLimits, AnOversizedServerReplyIsAMalformedCallNotACrash) {
    auto http = std::make_shared<FakeHttp>();
    http->routes[std::string("GET ") + kOrigin + "/v1/tool/state"] = {200, student_state(kMaxSteps + 1, 0)};
    LessonClient client(http);
    const auto r = client.state(kOrigin, kToken);
    EXPECT_FALSE(r.ok());
    EXPECT_TRUE(r.failure.kind == FailureKind::Malformed);
    EXPECT_FALSE(r.failure.detail.find(kToken) != std::string::npos);   // never the token
}
