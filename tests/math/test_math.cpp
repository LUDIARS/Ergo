#include "ergo/math/math.h"

#include "gtest/gtest.h"

#include <cmath>
#include <vector>

// M_PI は MSVC では <cmath> に入らない場合があるため定義する
#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Vec tests
// ============================================================================

TEST(Vec, AddSubMulDiv) {
    ergo::math::Vec2f a{};
    a.data[0] = 1.0f; a.data[1] = 2.0f;
    ergo::math::Vec2f b{};
    b.data[0] = 3.0f; b.data[1] = 4.0f;

    auto sum = a + b;
    EXPECT_FLOAT_EQ(sum.data[0], 4.0f);
    EXPECT_FLOAT_EQ(sum.data[1], 6.0f);

    auto diff = b - a;
    EXPECT_FLOAT_EQ(diff.data[0], 2.0f);
    EXPECT_FLOAT_EQ(diff.data[1], 2.0f);

    auto scaled = a * 3.0f;
    EXPECT_FLOAT_EQ(scaled.data[0], 3.0f);
    EXPECT_FLOAT_EQ(scaled.data[1], 6.0f);

    auto divided = b / 2.0f;
    EXPECT_FLOAT_EQ(divided.data[0], 1.5f);
    EXPECT_FLOAT_EQ(divided.data[1], 2.0f);

    auto comp_mul = a * b;
    EXPECT_FLOAT_EQ(comp_mul.data[0], 3.0f);
    EXPECT_FLOAT_EQ(comp_mul.data[1], 8.0f);
}

TEST(Vec, CompoundAssignment) {
    ergo::math::Vec2f v{};
    v.data[0] = 1.0f; v.data[1] = 2.0f;
    v += ergo::math::Vec2f{{3.0f, 4.0f}};
    EXPECT_FLOAT_EQ(v.data[0], 4.0f);
    EXPECT_FLOAT_EQ(v.data[1], 6.0f);

    v -= ergo::math::Vec2f{{1.0f, 1.0f}};
    EXPECT_FLOAT_EQ(v.data[0], 3.0f);
    EXPECT_FLOAT_EQ(v.data[1], 5.0f);

    v *= 2.0f;
    EXPECT_FLOAT_EQ(v.data[0], 6.0f);
    EXPECT_FLOAT_EQ(v.data[1], 10.0f);

    v /= 2.0f;
    EXPECT_FLOAT_EQ(v.data[0], 3.0f);
    EXPECT_FLOAT_EQ(v.data[1], 5.0f);
}

TEST(Vec, Dot) {
    ergo::math::Vec3f a{{1.0f, 2.0f, 3.0f}};
    ergo::math::Vec3f b{{4.0f, 5.0f, 6.0f}};
    EXPECT_FLOAT_EQ(a.dot(b), 32.0f);  // 4+10+18
}

TEST(Vec, LengthAndNormalize) {
    ergo::math::Vec2f v{{3.0f, 4.0f}};
    EXPECT_FLOAT_EQ(v.length_sq(), 25.0f);
    EXPECT_FLOAT_EQ(v.length(), 5.0f);

    auto n = v.normalized();
    EXPECT_NEAR(n.length(), 1.0f, 1e-6f);
    EXPECT_NEAR(n.data[0], 0.6f, 1e-6f);
    EXPECT_NEAR(n.data[1], 0.8f, 1e-6f);
}

TEST(Vec, NormalizeZeroVector) {
    ergo::math::Vec2f zero{};
    auto n = zero.normalized();
    EXPECT_FLOAT_EQ(n.data[0], 0.0f);
    EXPECT_FLOAT_EQ(n.data[1], 0.0f);
}

TEST(Vec, Lerp) {
    ergo::math::Vec2f a{{0.0f, 0.0f}};
    ergo::math::Vec2f b{{10.0f, 20.0f}};
    auto mid = a.lerp(b, 0.5f);
    EXPECT_FLOAT_EQ(mid.data[0], 5.0f);
    EXPECT_FLOAT_EQ(mid.data[1], 10.0f);
}

TEST(Vec, XYZAccessors) {
    ergo::math::Vec3f v{{1.0f, 2.0f, 3.0f}};
    EXPECT_FLOAT_EQ(v.x(), 1.0f);
    EXPECT_FLOAT_EQ(v.y(), 2.0f);
    EXPECT_FLOAT_EQ(v.z(), 3.0f);

    v.x() = 10.0f;
    EXPECT_FLOAT_EQ(v.data[0], 10.0f);

    ergo::math::Vec4f v4{{1.0f, 2.0f, 3.0f, 4.0f}};
    EXPECT_FLOAT_EQ(v4.w(), 4.0f);
}

TEST(Vec, ApproxEq) {
    ergo::math::Vec2f a{{1.0f, 2.0f}};
    ergo::math::Vec2f b{{1.0f + 1e-7f, 2.0f - 1e-7f}};
    EXPECT_TRUE(a.approx_eq(b, 1e-6f));
    EXPECT_FALSE(a.approx_eq(b, 1e-8f));
}

TEST(Vec, UnaryMinus) {
    ergo::math::Vec2f v{{3.0f, -4.0f}};
    auto neg = -v;
    EXPECT_FLOAT_EQ(neg.data[0], -3.0f);
    EXPECT_FLOAT_EQ(neg.data[1],  4.0f);
}

TEST(Vec, ScalarPreMul) {
    ergo::math::Vec2f v{{2.0f, 3.0f}};
    auto r = 4.0f * v;
    EXPECT_FLOAT_EQ(r.data[0], 8.0f);
    EXPECT_FLOAT_EQ(r.data[1], 12.0f);
}

// ============================================================================
// Mat tests
// ============================================================================

TEST(Mat, Identity) {
    auto id = ergo::math::Mat<3, 3, float>::identity();
    for (std::size_t r = 0; r < 3; ++r)
        for (std::size_t c = 0; c < 3; ++c)
            EXPECT_FLOAT_EQ(id[r][c], r == c ? 1.0f : 0.0f);
}

TEST(Mat, Multiply) {
    // [ 1 2 ] * [ 5 6 ] = [ 19 22 ]
    // [ 3 4 ]   [ 7 8 ]   [ 43 50 ]
    ergo::math::Mat<2, 2, float> a{};
    a[0][0]=1; a[0][1]=2;
    a[1][0]=3; a[1][1]=4;

    ergo::math::Mat<2, 2, float> b{};
    b[0][0]=5; b[0][1]=6;
    b[1][0]=7; b[1][1]=8;

    auto c = a * b;
    EXPECT_FLOAT_EQ(c[0][0], 19.0f);
    EXPECT_FLOAT_EQ(c[0][1], 22.0f);
    EXPECT_FLOAT_EQ(c[1][0], 43.0f);
    EXPECT_FLOAT_EQ(c[1][1], 50.0f);
}

TEST(Mat, Transpose) {
    ergo::math::Mat<2, 3, float> m{};
    m[0][0]=1; m[0][1]=2; m[0][2]=3;
    m[1][0]=4; m[1][1]=5; m[1][2]=6;

    auto t = m.transposed();
    EXPECT_FLOAT_EQ(t[0][0], 1.0f);
    EXPECT_FLOAT_EQ(t[1][0], 2.0f);
    EXPECT_FLOAT_EQ(t[2][0], 3.0f);
    EXPECT_FLOAT_EQ(t[0][1], 4.0f);
}

TEST(Mat, MatVecMultiply) {
    auto m = ergo::math::Mat<2, 2, float>::identity();
    ergo::math::Vec2f v{{3.0f, 4.0f}};
    auto r = m * v;
    EXPECT_FLOAT_EQ(r.data[0], 3.0f);
    EXPECT_FLOAT_EQ(r.data[1], 4.0f);
}

TEST(Mat2, InverseThenMat_EqualsIdentity) {
    ergo::math::Mat<2, 2, float> m{};
    m[0][0]=2; m[0][1]=1;
    m[1][0]=5; m[1][1]=3;

    auto inv = ergo::math::mat2_inverse(m);
    auto id  = m * inv;
    const float eps = 1e-5f;
    EXPECT_NEAR(id[0][0], 1.0f, eps); EXPECT_NEAR(id[0][1], 0.0f, eps);
    EXPECT_NEAR(id[1][0], 0.0f, eps); EXPECT_NEAR(id[1][1], 1.0f, eps);
}

TEST(Mat2, Rotation90) {
    float angle = static_cast<float>(M_PI) / 2.0f;
    auto rot90 = ergo::math::mat2_rotation(angle);
    ergo::math::Vec2f v{{1.0f, 0.0f}};
    auto rv = rot90 * v;
    EXPECT_NEAR(rv.data[0], 0.0f, 1e-6f);
    EXPECT_NEAR(rv.data[1], 1.0f, 1e-6f);
}

TEST(Mat3, InverseThenMat_EqualsIdentity) {
    ergo::math::Mat<3, 3, float> m{};
    m[0][0]=1; m[0][1]=2; m[0][2]=3;
    m[1][0]=0; m[1][1]=1; m[1][2]=4;
    m[2][0]=5; m[2][1]=6; m[2][2]=0;

    auto inv = ergo::math::mat3_inverse(m);
    auto id  = m * inv;
    const float eps = 1e-5f;
    for (std::size_t r = 0; r < 3; ++r)
        for (std::size_t c = 0; c < 3; ++c)
            EXPECT_NEAR(id[r][c], r == c ? 1.0f : 0.0f, eps);
}

TEST(Mat3, ApplyPointRoundtrip) {
    // Pure translation homography: [1, 0, tx; 0, 1, ty; 0, 0, 1]
    ergo::math::Mat<3, 3, float> m = ergo::math::Mat<3, 3, float>::identity();
    float tx = 5.0f, ty = -3.0f;
    m[0][2] = tx;
    m[1][2] = ty;

    ergo::math::Vec2f p{{2.0f, 7.0f}};
    auto out = ergo::math::mat3_apply_point(m, p);
    EXPECT_NEAR(out.data[0], p.data[0] + tx, 1e-5f);
    EXPECT_NEAR(out.data[1], p.data[1] + ty, 1e-5f);

    // Roundtrip through inverse
    auto inv = ergo::math::mat3_inverse(m);
    auto back = ergo::math::mat3_apply_point(inv, out);
    EXPECT_NEAR(back.data[0], p.data[0], 1e-5f);
    EXPECT_NEAR(back.data[1], p.data[1], 1e-5f);
}

TEST(Mat4, InverseThenMat_EqualsIdentity) {
    auto m = ergo::math::mat4_translation(1.0f, 2.0f, 3.0f)
           * ergo::math::mat4_scale(2.0f, 3.0f, 4.0f);
    auto inv = ergo::math::mat4_inverse(m);
    auto id  = m * inv;
    const float eps = 1e-4f;
    for (std::size_t r = 0; r < 4; ++r)
        for (std::size_t c = 0; c < 4; ++c)
            EXPECT_NEAR(id[r][c], r == c ? 1.0f : 0.0f, eps);
}

TEST(Mat4, TRS) {
    auto m = ergo::math::mat4_trs(
        1.0f, 0.0f, 0.0f,   // translate
        0.0f, 0.0f, 0.0f,   // rotate (none)
        2.0f, 2.0f, 2.0f    // scale
    );
    // origin -> (1,0,0,1) after TRS
    ergo::math::Vec4f origin{{0.0f, 0.0f, 0.0f, 1.0f}};
    auto t = m * origin;
    EXPECT_NEAR(t.data[0], 1.0f, 1e-5f);
    EXPECT_NEAR(t.data[1], 0.0f, 1e-5f);
    EXPECT_NEAR(t.data[2], 0.0f, 1e-5f);
}

// ============================================================================
// Batch tests
// ============================================================================

TEST(Batch, AddF32MatchesScalar) {
    const std::size_t N = 17;  // intentionally not a multiple of 4
    std::vector<float> a(N), b(N), out_simd(N), out_scalar(N);
    for (std::size_t i = 0; i < N; ++i) {
        a[i] = static_cast<float>(i) * 1.1f;
        b[i] = static_cast<float>(N - i) * 0.7f;
    }
    // scalar reference
    for (std::size_t i = 0; i < N; ++i) out_scalar[i] = a[i] + b[i];
    // SIMD (or scalar fallback)
    ergo::math::batch::add_f32(a.data(), b.data(), out_simd.data(), N);
    for (std::size_t i = 0; i < N; ++i)
        EXPECT_FLOAT_EQ(out_simd[i], out_scalar[i]);
}

TEST(Batch, ScaleF32MatchesScalar) {
    const std::size_t N = 13;
    std::vector<float> a(N), out(N);
    for (std::size_t i = 0; i < N; ++i) a[i] = static_cast<float>(i) + 1.0f;
    ergo::math::batch::scale_f32(a.data(), 3.5f, out.data(), N);
    for (std::size_t i = 0; i < N; ++i)
        EXPECT_FLOAT_EQ(out[i], a[i] * 3.5f);
}

TEST(Batch, MaddF32MatchesScalar) {
    const std::size_t N = 9;
    std::vector<float> a = {1,2,3,4,5,6,7,8,9};
    std::vector<float> b = {9,8,7,6,5,4,3,2,1};
    std::vector<float> a_ref = a;
    for (std::size_t i = 0; i < N; ++i) a_ref[i] += 2.0f * b[i];
    ergo::math::batch::madd_f32(a.data(), 2.0f, b.data(), N);
    for (std::size_t i = 0; i < N; ++i)
        EXPECT_FLOAT_EQ(a[i], a_ref[i]);
}

TEST(Batch, DotF32MatchesScalar) {
    const std::size_t N = 11;
    std::vector<float> a(N), b(N);
    for (std::size_t i = 0; i < N; ++i) {
        a[i] = static_cast<float>(i + 1);
        b[i] = static_cast<float>(N - i);
    }
    float ref = 0.0f;
    for (std::size_t i = 0; i < N; ++i) ref += a[i] * b[i];
    float got = ergo::math::batch::dot_f32(a.data(), b.data(), N);
    EXPECT_NEAR(got, ref, 1e-3f);
}

TEST(Batch, Add2F32SoA) {
    const std::size_t N = 8;
    std::vector<float> ax = {1,2,3,4,5,6,7,8};
    std::vector<float> ay = {8,7,6,5,4,3,2,1};
    std::vector<float> bx = {1,1,1,1,1,1,1,1};
    std::vector<float> by = {2,2,2,2,2,2,2,2};
    std::vector<float> ox(N), oy(N);
    ergo::math::batch::add2_f32(ax.data(), ay.data(),
                                 bx.data(), by.data(),
                                 ox.data(), oy.data(), N);
    for (std::size_t i = 0; i < N; ++i) {
        EXPECT_FLOAT_EQ(ox[i], ax[i] + bx[i]);
        EXPECT_FLOAT_EQ(oy[i], ay[i] + by[i]);
    }
}

// ============================================================================
// Pool tests
// ============================================================================

namespace {
struct TrackedObj {
    static int ctor_count;
    static int dtor_count;
    int value;

    explicit TrackedObj(int v) : value(v) { ++ctor_count; }
    ~TrackedObj() { ++dtor_count; }
};
int TrackedObj::ctor_count = 0;
int TrackedObj::dtor_count = 0;
}  // namespace

TEST(Arena, ConstructAndReset) {
    ergo::math::Arena arena(1024);
    TrackedObj::ctor_count = 0;
    TrackedObj::dtor_count = 0;

    auto* a = arena.construct<TrackedObj>(42);
    auto* b = arena.construct<TrackedObj>(99);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(a->value, 42);
    EXPECT_EQ(b->value, 99);
    EXPECT_EQ(TrackedObj::ctor_count, 2);

    // Objects are contiguous (b comes after a in the bump order)
    EXPECT_GT(reinterpret_cast<uintptr_t>(b),
              reinterpret_cast<uintptr_t>(a));

    // reset does NOT call destructors (by design — bulk reclaim)
    arena.reset();
    EXPECT_EQ(TrackedObj::dtor_count, 0);
    EXPECT_EQ(arena.used(), 0u);
}

TEST(Arena, OverflowReturnsNull) {
    ergo::math::Arena arena(8);  // very small
    // Allocate until full
    int alloc_count = 0;
    while (arena.construct<int>(0)) ++alloc_count;
    // At least one allocation must have succeeded
    EXPECT_GT(alloc_count, 0);
    // After overflow, nullptr
    EXPECT_EQ(arena.construct<int>(0), nullptr);
}

TEST(ObjectPool, CreateGetDestroy) {
    ergo::math::ObjectPool<TrackedObj> pool(4);
    TrackedObj::ctor_count = 0;
    TrackedObj::dtor_count = 0;

    auto h1 = pool.create(10);
    auto h2 = pool.create(20);
    EXPECT_NE(h1, ergo::math::ObjectPool<TrackedObj>::invalid_handle);
    EXPECT_NE(h2, ergo::math::ObjectPool<TrackedObj>::invalid_handle);
    EXPECT_EQ(pool.get(h1).value, 10);
    EXPECT_EQ(pool.get(h2).value, 20);
    EXPECT_EQ(TrackedObj::ctor_count, 2);
    EXPECT_EQ(pool.size(), 2u);

    pool.destroy(h1);
    EXPECT_EQ(TrackedObj::dtor_count, 1);
    EXPECT_FALSE(pool.alive(h1));
    EXPECT_EQ(pool.size(), 1u);
}

TEST(ObjectPool, FreeListReuse) {
    ergo::math::ObjectPool<TrackedObj> pool(2);
    auto h1 = pool.create(1);
    auto h2 = pool.create(2);
    // Pool full
    EXPECT_EQ(pool.create(3), ergo::math::ObjectPool<TrackedObj>::invalid_handle);

    pool.destroy(h1);
    // Reuse slot
    auto h3 = pool.create(3);
    EXPECT_NE(h3, ergo::math::ObjectPool<TrackedObj>::invalid_handle);
    EXPECT_EQ(pool.get(h3).value, 3);
    EXPECT_EQ(pool.size(), 2u);

    // Verify h2 is still alive
    EXPECT_EQ(pool.get(h2).value, 2);
}

TEST(ObjectPool, ContiguousSlab) {
    // All handles should refer to addresses within the slab.
    ergo::math::ObjectPool<float> pool(8);
    std::vector<ergo::math::ObjectPool<float>::Handle> handles;
    for (int i = 0; i < 8; ++i) handles.push_back(pool.create(static_cast<float>(i)));

    const float* base = pool.slab();
    for (std::size_t i = 0; i < 8; ++i) {
        EXPECT_GE(reinterpret_cast<uintptr_t>(&pool.get(handles[i])),
                  reinterpret_cast<uintptr_t>(base));
        EXPECT_LT(reinterpret_cast<uintptr_t>(&pool.get(handles[i])),
                  reinterpret_cast<uintptr_t>(base + 8));
    }
}
