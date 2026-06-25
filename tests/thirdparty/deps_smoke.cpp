// Smoke test for the managed third-party dependency manager.
//
// Proves that the fetched libraries are not just present but actually link and
// run: it exercises a kazmath vector operation and brings libcurl up/down. Built
// only when -DERGO_BUILD_THIRDPARTY_SMOKE=ON (which forces kazmath + curl on);
// see third_party/dependencies.cmake.

#include <cstdio>

#include <kazmath/vec3.h>
#include <curl/curl.h>

#include "gtest/gtest.h"

TEST(ThirdPartyKazmath, Vec3LengthIsComputed) {
    kmVec3 v;
    kmVec3Fill(&v, 3.0f, 4.0f, 0.0f);
    EXPECT_FLOAT_EQ(kmVec3Length(&v), 5.0f);

    kmVec3 n;
    kmVec3Normalize(&n, &v);
    EXPECT_FLOAT_EQ(kmVec3Length(&n), 1.0f);
}

TEST(ThirdPartyCurl, GlobalInitAndVersion) {
    ASSERT_EQ(curl_global_init(CURL_GLOBAL_DEFAULT), CURLE_OK);

    const curl_version_info_data* info = curl_version_info(CURLVERSION_NOW);
    ASSERT_NE(info, nullptr);
    EXPECT_NE(info->version, nullptr);

    CURL* handle = curl_easy_init();
    EXPECT_NE(handle, nullptr);
    if (handle) {
        curl_easy_cleanup(handle);
    }
    curl_global_cleanup();
}
