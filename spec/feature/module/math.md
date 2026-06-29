# math モジュール定義

## 概要

C++20 concepts ベースの**汎用数学ライブラリ**。
`ergo_physics2d` (2D 剛体物理) やスイカゲーム等の上位モジュールが載る下層。

DoD (Data-Oriented Design) 志向で設計されており、連続メモリプールと
SoA バッチ演算経路を提供する。
`union` による型 punning SIMD は採用しない (UB 寄り)。
SIMD は SoA バッチ経路に限定し、scalar フォールバックを必ず同梱する。

## カテゴリ

システム

## 所属ドメイン

数学 / 物理 / アロケーション基盤

## 依存

- C++20 標準ライブラリのみ (`<concepts>`, `<cmath>`, `<memory>`, `<new>`, `<vector>` 等)
- 他 Ergo モジュールへの依存なし
- `ergo::vector::Vec2 / Vec3` との相互変換は
  `ergo/math/vec_vector_convert.h` を明示インクルードで有効化 (opt-in)

## サブヘッダ構成

| ヘッダ | 内容 |
|---|---|
| `ergo/math/concepts.h` | `Scalar` (floating-point) / `Arithmetic` (fp + int) concept |
| `ergo/math/vec.h` | `Vec<N, T>` 汎用ベクトルテンプレート + 型エイリアス |
| `ergo/math/mat.h` | `Mat<R, C, T>` 汎用行列テンプレート + Mat2/3/4 ヘルパ |
| `ergo/math/batch.h` | SoA バッチ演算 (add/scale/madd/dot + 2D 版) |
| `ergo/math/pool.h` | `Arena` バンプアロケータ + `ObjectPool<T>` |
| `ergo/math/math.h` | 上記をまとめる umbrella ヘッダ |
| `ergo/math/vec_vector_convert.h` | `ergo::vector` 型との変換ヘルパ (opt-in) |

## API

### Vec<N, T>

- 要件: `Scalar T` (floating-point)
- `constexpr` operator+/-/*// (成分・スカラ)、+=/-=-=/*=//=
- `dot()`, `length_sq()`, `length()`, `normalized()`, `lerp()`, `approx_eq(eps)`
- `x()/y()/z()/w()` アクセサ (if constexpr で N>=1..4 のみ有効)
- 型エイリアス: `Vec2f/Vec3f/Vec4f` (float), `Vec2d/Vec3d/Vec4d` (double)

### Mat<R, C, T>

- 行優先 (row-major) ストレージ: `element(r, c) = data[r * C + c]`
- `operator[](r)` → 行ポインタ (use `m[r][c]`)
- `identity()` (正方行列のみ)、`transposed()`
- `operator*(Mat<C, K>)` (行列積)、`operator*(Vec<C>)` (行列×ベクトル)
- **Mat2 ヘルパ**: `mat2_rotation(theta)`, `mat2_det()`, `mat2_inverse()`
- **Mat3 ヘルパ**: `mat3_det()`, `mat3_inverse()`, `mat3_apply_point()` (同次座標 w=1)
- **Mat4 ヘルパ**: `mat4_translation/scale/rotation_x/y/z()`, `mat4_trs()`,
  `mat4_perspective()`, `mat4_ortho()`, `mat4_inverse()`

### batch (SoA バッチ演算)

ストレージレイアウト: **SoA** — `float x[], float y[]` (2D の場合) を
別配列で管理。interleaved `Vec2f[]` (AoS) ではない。
理由: SIMD gather/scatter 不要、物理/パーティクルソルバの自然なレイアウト。

- `add_f32(a, b, out, n)` — out[i] = a[i] + b[i]
- `scale_f32(a, s, out, n)` — out[i] = a[i] * s
- `madd_f32(a, s, b, n)` — a[i] += s * b[i] (in-place)
- `dot_f32(a, b, n)` — Σ a[i]*b[i]
- `add2/scale2/madd2/dot2_f32(...)` — 2D SoA 版 (x[], y[] ペア)

SSE2 経路: `#if ERGO_MATH_SIMD` で有効化 (x64 デフォルト ON)。
強制無効: コンパイル時 `-DERGO_MATH_SIMD=0`。

### pool

- **Arena**: 連続バイト列 + `std::align` によるアライメント済み bump alloc。
  `construct<T>(args...)` で placement new。`reset()` で全領域を一括回収
  (デストラクタ非呼び出し)。

- **ObjectPool\<T\>**: 固定容量連続スラブ + free-list。
  `create(args...)` → Handle、`destroy(h)` で返却 (デストラクタ呼び出し)。
  per-要素 heap alloc ゼロ。Handle は `std::size_t` のインデックス。

## テスト

- `Vec`: 加減乗除/dot/length/normalize/lerp、x()/y()/z() アクセサ、approx_eq
- `Mat`: 単位行列・乗算・転置・MatVec、Mat2/Mat3/Mat4 の逆行列 × 元 ≈ I
- `batch`: SSE2 有無に関わらず scalar ループと一致
- `pool`: Arena コンストラクタ呼び出しカウント・bump 順序・overflow 返却 null。
  ObjectPool create/get/destroy/free-list 再利用・slab 連続性

## 使用例

```cpp
#include "ergo/math/math.h"
using namespace ergo::math;

// Vec
Vec2f a{{1.0f, 0.0f}};
Vec2f b{{0.0f, 1.0f}};
float d = a.dot(b);          // 0.0
Vec2f n = (a + b).normalized(); // (√2/2, √2/2)

// Mat
auto rot = mat2_rotation(3.14159f / 4.0f);  // 45°
Vec2f rv = rot * a;                          // rotated

// Batch (SoA)
float px[1024], py[1024], vx[1024], vy[1024];
batch::madd2_f32(px, py, dt, vx, vy, 1024);  // px += dt*vx, py += dt*vy

// Pool
ObjectPool<RigidBody> pool(256);
auto h = pool.create(mass, pos);
pool.get(h).integrate(dt);
pool.destroy(h);
```
