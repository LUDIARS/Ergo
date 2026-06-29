# physics2d モジュール定義

## 概要

2D 剛体物理エンジン (Box2D ライク)。
衝突形状 (Circle / Polygon)、剛体 (Body)、シミュレーション世界 (World)、
接触イベント (ContactEvent) を実装する。

`ergo_math` の `ObjectPool<Body>` を用いた DoD (Data-Oriented Design) 配置で
メモリ局所性を確保。ジョイント・CCD・島分割なし (数十剛体規模のゲーム想定)。

典型ユースケース: スイカゲーム型の落下・合体物理、ピンボール、2D プラットフォーマー
の地形衝突判定。

## カテゴリ

システム

## 所属ドメイン

物理 / 衝突判定

## 依存

- `ergo_math` (Vec/Mat/ObjectPool)
- C++20 標準ライブラリ (`<cmath>`, `<set>`, `<vector>`, `<algorithm>` 等)
- 他 Ergo モジュールへの依存なし

## データ型

### Shape (ergo/physics2d/shape.h)

```cpp
constexpr int MAX_POLYGON_VERTS = 8;

struct Circle  { float radius; };
struct Polygon { Vec<2,float> verts[8]; int count; };

enum class ShapeType { Circle, Polygon };
struct Shape {
    ShapeType type;
    union { Circle circle; Polygon polygon; };
};

Polygon make_box(float half_w, float half_h);   // CCW 矩形
Shape make_circle_shape(float radius);
Shape make_box_shape(float half_w, float half_h);
```

### Body (ergo/physics2d/body.h)

```cpp
using BodyHandle = uint32_t;
constexpr BodyHandle INVALID_BODY = UINT32_MAX;
enum class BodyType { Static, Dynamic, Kinematic };

struct BodyDef {
    BodyType type = Dynamic; Vec<2,float> position, linear_velocity;
    float angle=0, angular_velocity=0, restitution=0.3, friction=0.5, density=1;
    float linear_damping=0;   // per-body linear drag coefficient (>= 0)
    float angular_damping=0;  // per-body angular drag coefficient (>= 0)
    uint64_t user_data = 0;
};

struct Body {
    Vec<2,float> position, linear_velocity;
    float angle, angular_velocity;
    float mass, inv_mass, inertia, inv_inertia;
    float restitution, friction;
    float linear_damping;   // copied from BodyDef
    float angular_damping;  // copied from BodyDef
    uint64_t user_data;
    BodyType type;
    Shape shape;
};

void compute_mass_and_inertia(Body&, float density);
```

**質量計算:**
- Circle: `area = π·r²`, `mass = density·area`, `I = 0.5·mass·r²`
- Polygon: Shoelace 公式で面積、三角ファン分割で慣性モーメント
- Static / Kinematic: `inv_mass = 0, inv_inertia = 0`

### ContactEvent (ergo/physics2d/contact.h)

```cpp
enum class ContactState { Begin, Stay, End };
struct ContactEvent { BodyHandle a, b; uint64_t user_data_a, user_data_b; ContactState state; };

struct Manifold {
    BodyHandle body_a, body_b;
    Vec<2,float> normal; float penetration;
    Vec<2,float> contact_points[2]; int contact_count;
    float restitution, friction;
    float accumulated_normal[2], accumulated_tangent[2]; // warm-start
};
```

### World (ergo/physics2d/world.h)

```cpp
class World {
public:
    explicit World(Vec<2,float> gravity = {{0,-9.81}});
    BodyHandle create_body(const BodyDef&, const Shape&);
    void destroy_body(BodyHandle);
    void step(float dt, int vel_iter=8, int pos_iter=3);
    Body* get_body(BodyHandle);              // nullptr if invalid
    template<Fn> void for_each_body(Fn&&);  // (BodyHandle, Body&)
    const vector<ContactEvent>& get_contact_events() const;
private:
    ObjectPool<Body> pool_;   // WORLD_MAX_BODIES = 1024
    // N² AABB broadphase
    // contact event tracking (set<pair<BodyHandle,BodyHandle>>)
};
```

## アルゴリズム

### Semi-implicit Euler 積分

```
velocity += gravity * dt                        (Dynamic のみ)
velocity *= 1 / (1 + dt * linear_damping)      (per-body、フレームレート非依存)
angular_velocity *= 1 / (1 + dt * angular_damping)
position += velocity * dt
angle    += angular_velocity * dt
```

`linear_damping` / `angular_damping` は Box2D 準拠の per-body 係数 (デフォルト 0.0)。
damping = 0 のとき式は乗数 1.0 となりノーオペレーション。
旧来の接触ベース hardcode damping (`ang_damp=0.85`, `lin_damp=0.995`) は廃止。
減衰が必要なシナリオでは BodyDef にフィールドを明示設定すること。

### N² AABB ブロードフェーズ

全生存ボディペアを O(n²) で走査し AABB オーバーラップを確認。
BVH/Sweep-and-Prune への差し替えポイントは `broadphase_and_solve()` 内。

### SAT ナローフェーズ

| ペア | 手法 |
|---|---|
| Circle–Circle | 距離 vs 半径和 |
| Circle–Polygon | 多角形最近傍点 + 内外判定 |
| Polygon–Polygon | SAT 全辺法線 + Sutherland-Hodgman クリッピング接触点 |

### Sequential Impulse ソルバ

各接触に対して velocity iteration を繰り返す:

1. 接触点の相対速度 `Vrel = vb - va + (ωb×rb - ωa×ra)`
2. 法線インパルス: `j = -(1+e)·Vrel·n / (inv_mass_sum + 回転項)`
3. 累積インパルスを `[0, ∞]` にクランプ (分離方向インパルスを防ぐ)
4. 摩擦インパルス: 接線方向、Coulomb クランプ `|jt| ≤ μ·|jn|`
5. Baumgarte 位置補正: `correction = normal * β * (penetration - slop) / inv_mass_sum`

### 接触イベント追跡

ステップ毎に `set<pair<BodyHandle,BodyHandle>>` で前ステップの接触集合を保持し、
新規 → Begin、継続 → Stay、消失 → End を判定して `contact_events_` に格納。

## API

```cpp
#include "ergo/physics2d/physics2d.h"
using namespace ergo::physics2d;

World world({{0.0f, -9.81f}});

// 静的床
BodyDef floor; floor.type = BodyType::Static; floor.position = {{0,0}};
world.create_body(floor, make_box_shape(10.0f, 0.5f));

// 動的ボール
BodyDef ball; ball.position = {{0, 5}}; ball.restitution = 0.5f;
BodyHandle h = world.create_body(ball, make_circle_shape(0.5f));

float dt = 1.0f / 60.0f;
for (int i = 0; i < 300; ++i) {
    world.step(dt);
    for (auto& ev : world.get_contact_events()) {
        if (ev.state == ContactState::Begin)
            // 衝突開始処理
    }
}
Body* b = world.get_body(h);
// b->position, b->linear_velocity ...
```

## テスト (tests/physics2d/test_physics2d.cpp)

| テスト名 | 検証内容 |
|---|---|
| FreeFall | 自由落下後の y 座標が運動方程式と一致 (±0.5m) |
| LandOnFloor | 静的床に着地後 y≈1.0、速度≈0 に収束 |
| CirclesRepel | 重なった 2 円が 1 秒後に分離 (間隔 > 0.9m) |
| Restitution | e=1 の弾性衝突後に上向き速度が生じる |
| Friction | 摩擦係数 0.8 の面上で 3 秒後に横速度 < 1 m/s |
| ContactBeginEvent | 衝突時に Begin イベントが user_data 付きで発火 |
| StackStability | 5 個積み上げ後に爆発せず位置が有限範囲に収まる |

## 使用例 (スイカゲーム型)

```cpp
// 異なるサイズの円を落下させ接触イベントで合体処理
World world({{0, -9.81f}});
// ... 左右壁 / 底面を Static ボディで構築 ...
for (auto& ev : world.get_contact_events()) {
    if (ev.state == ContactState::Begin &&
        same_tier(ev.user_data_a, ev.user_data_b)) {
        world.destroy_body(ev.a);
        world.destroy_body(ev.b);
        // 上位サイズの円を新規作成
    }
}
```

## スコープ外 (Out of Scope)

- ジョイント (RevoluteJoint / PrismaticJoint 等)
- 連続衝突検出 (CCD / Tunneling 防止)
- 島分割 (スリープ最適化)
- BVH / Dynamic AABB Tree broadphase (N² のまま)
- 3D 物理
