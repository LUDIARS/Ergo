// Minimal Google Test compatible header for environments without network access.
// Supports: TEST, TEST_F, EXPECT_*, ASSERT_*, SetUp/TearDown fixtures.
#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include <type_traits>

namespace testing {

struct TestInfo {
    std::string suite;
    std::string name;
    std::function<void()> factory;
};

inline std::vector<TestInfo>& registry() {
    static std::vector<TestInfo> r;
    return r;
}

inline int& failCount() { static int c = 0; return c; }
inline int& totalAssertions() { static int c = 0; return c; }

struct Test {
    virtual ~Test() = default;
    virtual void SetUp() {}
    virtual void TearDown() {}
    virtual void TestBody() = 0;
};

struct RegisterHelper {
    RegisterHelper(const char* suite, const char* name, std::function<void()> f) {
        registry().push_back({suite, name, std::move(f)});
    }
};

inline int runAllTests() {
    int passed = 0, failed = 0;
    for (auto& t : registry()) {
        std::printf("[ RUN      ] %s.%s\n", t.suite.c_str(), t.name.c_str());
        int before = failCount();
        try {
            t.factory();
        } catch (const std::exception& e) {
            std::printf("  Exception: %s\n", e.what());
            failCount()++;
        } catch (...) {
            std::printf("  Unknown exception\n");
            failCount()++;
        }
        if (failCount() > before) {
            std::printf("[   FAILED ] %s.%s\n", t.suite.c_str(), t.name.c_str());
            ++failed;
        } else {
            std::printf("[       OK ] %s.%s\n", t.suite.c_str(), t.name.c_str());
            ++passed;
        }
    }
    std::printf("\n[==========] %d tests ran.\n", passed + failed);
    std::printf("[  PASSED  ] %d tests.\n", passed);
    if (failed) std::printf("[  FAILED  ] %d tests.\n", failed);
    return failed ? 1 : 0;
}

} // namespace testing

// Assertion helpers
#define MINI_GTEST_ASSERT_IMPL(cond, condStr, fatal) \
    do { \
        testing::totalAssertions()++; \
        if (!(cond)) { \
            std::printf("  %s:%d: Failure\n  Expected: %s\n", __FILE__, __LINE__, condStr); \
            testing::failCount()++; \
            if (fatal) return; \
        } \
    } while(0)

#define EXPECT_TRUE(c)  MINI_GTEST_ASSERT_IMPL((c), #c, false)
#define EXPECT_FALSE(c) MINI_GTEST_ASSERT_IMPL(!(c), "!(" #c ")", false)
#define ASSERT_TRUE(c)  MINI_GTEST_ASSERT_IMPL((c), #c, true)
#define ASSERT_FALSE(c) MINI_GTEST_ASSERT_IMPL(!(c), "!(" #c ")", true)

#define MINI_GTEST_COMPARE(a, b, op, opStr, fatal) \
    do { \
        testing::totalAssertions()++; \
        auto&& _a = (a); auto&& _b = (b); \
        if (!(_a op _b)) { \
            std::printf("  %s:%d: Failure\n  Expected: %s %s %s\n", \
                        __FILE__, __LINE__, #a, opStr, #b); \
            testing::failCount()++; \
            if (fatal) return; \
        } \
    } while(0)

#define EXPECT_EQ(a, b) MINI_GTEST_COMPARE(a, b, ==, "==", false)
#define EXPECT_NE(a, b) MINI_GTEST_COMPARE(a, b, !=, "!=", false)
#define EXPECT_LT(a, b) MINI_GTEST_COMPARE(a, b, <,  "<",  false)
#define EXPECT_LE(a, b) MINI_GTEST_COMPARE(a, b, <=, "<=", false)
#define EXPECT_GT(a, b) MINI_GTEST_COMPARE(a, b, >,  ">",  false)
#define EXPECT_GE(a, b) MINI_GTEST_COMPARE(a, b, >=, ">=", false)
#define ASSERT_EQ(a, b) MINI_GTEST_COMPARE(a, b, ==, "==", true)
#define ASSERT_NE(a, b) MINI_GTEST_COMPARE(a, b, !=, "!=", true)

#define EXPECT_FLOAT_EQ(a, b) \
    do { \
        testing::totalAssertions()++; \
        if (std::abs(static_cast<float>(a) - static_cast<float>(b)) > 1e-5f) { \
            std::printf("  %s:%d: Failure\n  EXPECT_FLOAT_EQ(%s, %s)\n  Actual: %f vs %f\n", \
                        __FILE__, __LINE__, #a, #b, (double)(a), (double)(b)); \
            testing::failCount()++; \
        } \
    } while(0)

#define EXPECT_DOUBLE_EQ(a, b) \
    do { \
        testing::totalAssertions()++; \
        if (std::abs(static_cast<double>(a) - static_cast<double>(b)) > 1e-9) { \
            std::printf("  %s:%d: Failure\n  EXPECT_DOUBLE_EQ(%s, %s)\n  Actual: %f vs %f\n", \
                        __FILE__, __LINE__, #a, #b, (double)(a), (double)(b)); \
            testing::failCount()++; \
        } \
    } while(0)

#define EXPECT_NEAR(a, b, eps) \
    do { \
        testing::totalAssertions()++; \
        if (std::abs(static_cast<double>(a) - static_cast<double>(b)) > static_cast<double>(eps)) { \
            std::printf("  %s:%d: Failure\n  EXPECT_NEAR(%s, %s, %s)\n  Actual: %f vs %f\n", \
                        __FILE__, __LINE__, #a, #b, #eps, (double)(a), (double)(b)); \
            testing::failCount()++; \
        } \
    } while(0)

// TEST macro
#define TEST(Suite, Name) \
    static void Suite##_##Name##_body(); \
    static testing::RegisterHelper Suite##_##Name##_reg(#Suite, #Name, Suite##_##Name##_body); \
    static void Suite##_##Name##_body()

// TEST_F macro - derives from fixture and calls SetUp/TearDown via TestBody wrapper
#define TEST_F(Fixture, Name) \
    struct Fixture##_##Name##_cls : public Fixture { \
        void TestBody(); \
        void RunTest() { \
            this->SetUp(); \
            this->TestBody(); \
            this->TearDown(); \
        } \
    }; \
    static void Fixture##_##Name##_run() { \
        Fixture##_##Name##_cls t; \
        t.RunTest(); \
    } \
    static testing::RegisterHelper Fixture##_##Name##_reg(#Fixture, #Name, Fixture##_##Name##_run); \
    void Fixture##_##Name##_cls::TestBody()

// Main
inline int RUN_ALL_TESTS() { return testing::runAllTests(); }
