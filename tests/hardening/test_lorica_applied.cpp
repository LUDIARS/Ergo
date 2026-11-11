// tests/hardening/test_lorica_applied.cpp - is this translation unit really
// built with Lorica? (spec/setup/hardening.md, "Lorica applied" tests)
//
// Built twice by cmake/ErgoHardeningChecks.cmake, through the same
// ergo_harden_targets() walk as every other Ergo target:
//   test_lorica_applied           baseline profile
//   test_lorica_applied_extended  marked network-facing, so EXTENDED when the
//                                 toolchain has it (ERGO_HARDENING_PROFILE)
// The configure-time coverage check (ergo_hardening_verify) only reads target
// properties; this test looks at what the compiler actually saw. The command
// lines of every unit are checked by test_lorica_compile_commands.
//
// Defines from CMake (expectations, not results):
//   ERGO_LORICA_ACTIVE             Lorica's build profiles apply here (else: boundary checks, then skip 77)
//   ERGO_LORICA_EXPECT_UNSAFE_API  lorica_deprecate_unsafe_apis applies here
//   ERGO_LORICA_EXPECT_EXTENDED    this target should carry the EXTENDED profile
//   ERGO_LORICA_EXTENDED_UNAVAILABLE  the EXTENDED variant on a toolchain without
//                                  EXTENDED (skip, 77: it would only repeat the baseline)
//   ERGO_LORICA_OPTIMIZED_CONFIG   Release / RelWithDebInfo / MinSizeRel, where
//                                  Lorica adds _FORTIFY_SOURCE (LoricaHardening.cmake)

// --- Probe for the force-included <lorica/unsafe_api.hpp> --------------------
// Must stay above every #include. The header has no include guard macro
// (#pragma once) and #undefs its helper macros, so its mark is what it leaves
// behind: ::strcpy declared before any C library header. Without the forced
// include nothing declares strcpy at this point, and the lookup below finds
// only the fallback template (overload resolution prefers a matching
// non-template). Lorica's header refuses to come after <string.h>, so a
// declaration from the C library itself cannot be mistaken for it here.
#if !defined(_WIN32)
#if defined(_STRING_H) || defined(_STDIO_H) || defined(_WCHAR_H)
#define ERGO_PROBE_C_HEADER_FIRST 1
#else
#define ERGO_PROBE_C_HEADER_FIRST 0
#endif
namespace ergo_lorica_probe {
struct absent {};
template <class... A> absent strcpy(A...);
template <class T> T arg();  // unevaluated stand-in (no <utility> yet, and no null argument)
}  // namespace ergo_lorica_probe
namespace ergo_lorica_probe_lookup {
using namespace ergo_lorica_probe;
#if defined(__GNUC__)
#pragma GCC diagnostic push
// Naming the deprecated declaration is the point of the probe.
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
using strcpy_result = decltype(strcpy(arg<char*>(), arg<const char*>()));
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
}  // namespace ergo_lorica_probe_lookup
#endif

#include <cstddef>
#include <cstdio>
#include <limits>
#include <type_traits>
#include <vector>  // brings in the standard library's configuration macros

#include "gtest/gtest.h"

// Lorica::boundary is linked everywhere, also where Lorica's build profiles do
// not apply (ergo_harden_targets), so the headers are always there.
#include <lorica/checked_size.hpp>
#include <lorica/input_budget.hpp>

#if ERGO_LORICA_ACTIVE

#if defined(_LIBCPP_VERSION)
// Lorica passes -D_LIBCPP_HARDENING_MODE=FAST, or EXTENSIVE with EXTENDED.
static bool defined_libcpp_hardening_mode() {
#if !defined(_LIBCPP_HARDENING_MODE)
    return false;
#elif ERGO_LORICA_EXPECT_EXTENDED
    return _LIBCPP_HARDENING_MODE == _LIBCPP_HARDENING_MODE_EXTENSIVE;
#else
    return _LIBCPP_HARDENING_MODE == _LIBCPP_HARDENING_MODE_FAST;
#endif
}
#endif

TEST(LoricaApplied, UnsafeApiHeaderForceIncluded) {
#if defined(_WIN32)
    // MSVC: Lorica does not force-include the header there; it turns the UCRT's
    // own deprecation (C4996) into an error with /we4996, which leaves no mark a
    // unit can see. The Visual Studio generator writes no compile_commands.json
    // either, so on Windows only the configure-time property check covers it.
    std::printf("  note: unsafe API rejection is /we4996 on MSVC (not visible from inside a unit)\n");
#elif ERGO_LORICA_EXPECT_UNSAFE_API
    EXPECT_FALSE(ERGO_PROBE_C_HEADER_FIRST);
    constexpr bool declared =
        !std::is_same<ergo_lorica_probe_lookup::strcpy_result, ergo_lorica_probe::absent>::value;
    EXPECT_TRUE(declared);  // false: <lorica/unsafe_api.hpp> was not force-included
#else
    std::printf("  note: unsafe API rejection does not apply to this toolchain (configure STATUS says why)\n");
#endif
}

TEST(LoricaApplied, StandardLibraryChecks) {
#if defined(_LIBCPP_VERSION)
    ASSERT_TRUE(defined_libcpp_hardening_mode());
#elif defined(__GLIBCXX__)
#if !defined(_GLIBCXX_ASSERTIONS)
    EXPECT_TRUE(false);  // Lorica defines _GLIBCXX_ASSERTIONS for every C++ unit
#endif
#elif defined(_MSC_VER)
    // MSVC baseline (/GS /sdl /guard:cf) defines nothing; EXTENDED is below.
    std::printf("  note: MSVC baseline flags define no macro\n");
#else
    EXPECT_TRUE(false);  // a standard library Lorica's profiles do not know
#endif
}


TEST(LoricaApplied, FortifySource) {
#if defined(_MSC_VER) && !defined(__clang__)
    std::printf("  note: _FORTIFY_SOURCE is a glibc mechanism (not on MSVC)\n");
#elif ERGO_LORICA_OPTIMIZED_CONFIG
    // Lorica asks for level 3 from GCC / Clang 12 on and 2 before that
    // (_lorica_linux_capabilities). Ubuntu's GCC predefines 3 on its own, so on
    // that compiler this line alone cannot tell; the command line check can.
#if (defined(__clang__) && __clang_major__ >= 12) || (!defined(__clang__) && defined(__GNUC__) && __GNUC__ >= 12)
    constexpr int expected = 3;
#else
    constexpr int expected = 2;
#endif
#if defined(_FORTIFY_SOURCE)
    EXPECT_EQ(_FORTIFY_SOURCE, expected);
#else
    EXPECT_EQ(0, expected);  // _FORTIFY_SOURCE is not defined at all
#endif
#else
    // Debug (the ASan / TSan presets): Lorica adds FORTIFY only to the
    // optimized configurations, where the C library honours it.
    std::printf("  note: not an optimized configuration, Lorica adds no _FORTIFY_SOURCE\n");
#endif
}

TEST(LoricaApplied, ExtendedProfile) {
#if !ERGO_LORICA_EXPECT_EXTENDED
    std::printf("  note: baseline target\n");
#elif defined(_MSC_VER) && !defined(__clang__)
#if !defined(_MSVC_STL_HARDENING) || !defined(_MSVC_STL_DESTRUCTOR_TOMBSTONES)
    EXPECT_TRUE(false);  // EXTENDED defines both on MSVC
#else
    EXPECT_EQ(_MSVC_STL_HARDENING, 1);
    EXPECT_EQ(_MSVC_STL_DESTRUCTOR_TOMBSTONES, 1);
#endif
#elif defined(_LIBCPP_VERSION)
    EXPECT_TRUE(defined_libcpp_hardening_mode());  // EXTENSIVE
#else
    // libstdc++: EXTENDED adds -ftrivial-auto-var-init=zero, which defines no
    // macro, and reading an uninitialised variable to see it would be undefined
    // behaviour. test_lorica_compile_commands checks the flag on this target.
    std::printf("  note: EXTENDED on libstdc++ is checked on the command line\n");
#endif
}

#endif  // ERGO_LORICA_ACTIVE

// Lorica::boundary is header-only: the include path it carries is what links
// it in. Run the checks at run time (volatile inputs), not as constants. These
// run in every build: the parsers' input limits depend on them.
TEST(LoricaApplied, CheckedAddRejectsOverflow) {
    volatile std::size_t max = std::numeric_limits<std::size_t>::max();
    volatile std::size_t one = 1;
    EXPECT_FALSE(lorica::checked_add(max, one).has_value());
    auto fits = lorica::checked_add(max - one, one);
    ASSERT_TRUE(fits.has_value());
    EXPECT_EQ(*fits, std::numeric_limits<std::size_t>::max());
}

TEST(LoricaApplied, InputBudgetRefusesOnePastTheLimit) {
    volatile std::size_t limit = 4096;
    lorica::input_budget over(lorica::budget_limits{limit, limit, limit, limit});
    EXPECT_FALSE(over.consume(limit + 1, 0, 0));
    EXPECT_FALSE(over.consume(0, limit + 1, 0));
    EXPECT_FALSE(over.consume(0, 0, limit + 1));
    EXPECT_FALSE(over.permits_depth(limit + 1));

    lorica::input_budget exact(lorica::budget_limits{limit, limit, limit, limit});
    EXPECT_TRUE(exact.permits_depth(limit));
    EXPECT_TRUE(exact.consume(limit, limit, limit));
    EXPECT_FALSE(exact.consume(1, 0, 0));  // spent
}

int main() {
#if !ERGO_LORICA_ACTIVE
    // macOS / iOS / Android / MinGW or CMake older than 3.24: no Lorica build
    // flags there (configure WARNING). The boundary checks above still run and
    // fail the test if Lorica's headers are wrong; the flag checks cannot, so
    // ctest counts a skip, never a pass.
    if (const int rc = RUN_ALL_TESTS(); rc != 0) return rc;
    std::printf("SKIPPED: Lorica::boundary checked; Lorica's build hardening is not active in this build "
                "(see the configure WARNING), so the flag checks did not run\n");
    return 77;
#elif ERGO_LORICA_EXTENDED_UNAVAILABLE
    std::printf("SKIPPED: Lorica's EXTENDED profile is not available with this toolchain (configure STATUS says why)\n");
    return 77;
#else
    return RUN_ALL_TESTS();
#endif
}
