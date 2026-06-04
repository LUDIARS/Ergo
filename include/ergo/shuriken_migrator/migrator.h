#pragma once

/// Shuriken Migrator — Unity ParticleSystem (a.k.a. Shuriken) を
/// `ergo::particle::ParticleEffectConfig` に変換するライブラリ。
///
/// Unity の `.prefab` / `.unity` シーンファイルは YAML テキスト。 ParticleSystem
/// は file ID 198 のブロックとして埋め込まれている。 本モジュールは依存ゼロの
/// 最小 YAML スキャナで該当ブロックを抽出し、 主要フィールド (startLifetime,
/// startSpeed, startSize, startColor, gravityModifier, rateOverTime, shape.radius
/// など) を ergo の 2D エフェクト設定に写像する。
///
/// 移植不可 / 部分対応の項目は `MigrationReport.warnings` に文字列で記録される。

#include "ergo/particle/effect_config.h"
#include "ergo/gpu_particle/emitter_descriptor.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ergo::shuriken_migrator {

enum class MinMaxState : int {
    Constant     = 0,
    Curve        = 1,
    TwoConstants = 2,
    TwoCurves    = 3,
};

/// Unity の MinMaxCurve を表現。 state によって参照すべき値が変わる。
struct MinMaxCurve {
    MinMaxState state = MinMaxState::Constant;
    float scalar    = 0.0f;  // state == Constant
    float minScalar = 0.0f;  // state == TwoConstants
    float maxScalar = 0.0f;  // state == TwoConstants
    float scalarMultiplier = 1.0f;

    /// 統一インターフェース: 範囲 [min, max] を取り出す。
    void Range(float& outMin, float& outMax) const;
};

struct MinMaxGradient {
    MinMaxState state = MinMaxState::Constant;
    float startColor[4] = {1, 1, 1, 1};
    float endColor[4]   = {1, 1, 1, 1};
};

struct ShurikenMain {
    float duration = 5.0f;
    bool  looping = true;
    MinMaxCurve startLifetime;
    MinMaxCurve startSpeed;
    MinMaxCurve startSize;
    float startColor[4] = {1, 1, 1, 1};
    MinMaxCurve gravityModifier;
    int simulationSpace = 0;  // 0 = Local, 1 = World
    int maxNumParticles = 1000;
};

struct ShurikenBurst {
    float       time = 0.0f;
    MinMaxCurve count;
    uint32_t    cycles = 1;
    float       interval = 0.01f;
    float       probability = 1.0f;
};

struct ShurikenEmission {
    bool enabled = true;
    MinMaxCurve rateOverTime;
    std::vector<ShurikenBurst> bursts;
};

struct ShurikenShape {
    bool enabled = true;
    int  shapeType = 0;  // 0=Sphere 4=Hemisphere 5=Cone 6=Box ...
    float radius = 1.0f;
    float angle  = 25.0f;
    float scale[3] = {1, 1, 1};
};

struct ShurikenColorOverLifetime {
    bool enabled = false;
    MinMaxGradient gradient;
};

struct ShurikenSizeOverLifetime {
    bool enabled = false;
    MinMaxCurve size;
};

// ---- Shuriken 拡張モジュール (gpu_particle::EmitterDescriptor 向け) ----

struct ShurikenVelocityOverLifetime {
    bool        enabled = false;
    MinMaxCurve x, y, z;
};

struct ShurikenLimitVelocityOverLifetime {
    bool        enabled = false;
    MinMaxCurve speed;
    float       dampen = 0.0f;
    float       drag   = 0.0f;
};

struct ShurikenNoiseModule {
    bool   enabled = false;
    float  strength = 1.0f;
    float  frequency = 0.5f;
    int    octaves = 1;
    float  octave_multiplier = 0.5f;
    float  octave_scale = 2.0f;
    bool   separate_axes = false;
    float  scroll_speed = 0.0f;
};

struct ShurikenTrails {
    bool   enabled = false;
    float  lifetime = 0.5f;
    float  width    = 0.1f;
    float  color_start[4] = {1, 1, 1, 1};
    float  color_end[4]   = {1, 1, 1, 0};
    float  min_vertex_distance = 0.1f;
    bool   inherit_particle_color = true;
    bool   die_with_particle      = true;
};

struct ShurikenSubEmitter {
    int    event = 1;          // 0=Birth 1=Death 2=Collision
    int    emitter_index = 0;
    float  probability = 1.0f;
};

struct ShurikenTextureSheetAnimation {
    bool   enabled = false;
    int    num_tiles_x = 1;
    int    num_tiles_y = 1;
    int    time_mode   = 0;    // 0=Lifetime 1=Speed 2=FPS
    float  fps = 30.0f;
    bool   random_row = false;
    int    cycles = 1;
};

struct ShurikenSource {
    std::string name;                     // GameObject の m_Name (推測 best-effort)
    ShurikenMain main;
    ShurikenEmission emission;
    ShurikenShape shape;
    ShurikenColorOverLifetime colorOverLifetime;
    ShurikenSizeOverLifetime sizeOverLifetime;

    // 拡張 (gpu_particle 向け)
    ShurikenVelocityOverLifetime       velocityOverLifetime;
    ShurikenLimitVelocityOverLifetime  limitVelocityOverLifetime;
    ShurikenNoiseModule                noise;
    ShurikenTrails                     trails;
    std::vector<ShurikenSubEmitter>    subEmitters;
    ShurikenTextureSheetAnimation      textureSheet;
};

struct MigrationReport {
    std::vector<std::string> warnings;     // 警告 (部分対応 / 値クランプ等)
    std::vector<std::string> unsupported;  // 完全に未対応の機能名
    int extractedSystems = 0;              // 抽出した ParticleSystem 数
    int convertedSystems = 0;              // 変換成功数

    void Warn(std::string msg) { warnings.emplace_back(std::move(msg)); }
    void Unsupp(std::string name) { unsupported.emplace_back(std::move(name)); }
};

// ---- 階層付き抽出 (子オブジェクト探索 + ツリー保持) --------------------

/// Unity Transform の local TRS。 rotation は quaternion (xyzw)。
struct ShurikenTransform {
    float local_position[3] = {0, 0, 0};
    float local_rotation[4] = {0, 0, 0, 1};  // xyzw
    float local_scale[3]    = {1, 1, 1};
};

/// prefab の GameObject ツリーの 1 ノード。 ParticleSystem を持つ GameObject も
/// あれば、 単なるグループ (空 GameObject) もある。 children でツリーを保持する。
struct ShurikenNode {
    std::string               name;
    ShurikenTransform         transform;
    bool                      has_emitter = false;
    ShurikenSource            emitter;        ///< has_emitter == true のとき有効
    std::vector<ShurikenNode> children;
};

/// gpu_particle 変換結果の 1 ノード (ツリーを flatten した配列形式)。
/// out[0] が root。 parent_index は同じ配列内の親 index (-1 = root)。
struct GpuEmitterNode {
    std::string name;
    float       local_position[3] = {0, 0, 0};
    float       local_rotation[4] = {0, 0, 0, 1};  // xyzw quaternion
    float       local_scale[3]    = {1, 1, 1};
    int         parent_index      = -1;
    bool        has_emitter       = false;
    ergo::gpu_particle::EmitterDescriptor emitter;  ///< has_emitter のとき有効
};

// ---- Public API --------------------------------------------------------

/// Unity prefab/scene の YAML 文字列から ParticleSystem を全て抽出する (flat)。
bool ParsePrefabYaml(const std::string& yamlText,
                     std::vector<ShurikenSource>& outSystems,
                     MigrationReport& report);

/// prefab/scene YAML から GameObject/Transform 階層を復元し、 子オブジェクトを
/// 再帰的に探索して全 ParticleSystem をツリー構造を保ったまま抽出する。
/// outRoot は prefab のルート (ルートが複数あれば合成ルートにまとめる)。
/// 注: prefab-instance (`!u!1001`) 経由の stripped component は展開しない
///     (元 prefab を別途読む必要がある旨を report に記録する)。
bool ParsePrefabTree(const std::string& yamlText,
                     ShurikenNode& outRoot,
                     MigrationReport& report);

/// ShurikenSource を Ergo 2D 用設定 (particle::ParticleEffectConfig) に変換する。
ergo::particle::ParticleEffectConfig ConvertToErgo(const ShurikenSource& src,
                                                   MigrationReport& report);

/// ShurikenSource を gpu_particle::EmitterDescriptor に変換する。
/// 2D の ParticleEffectConfig より広い Shuriken 機能 (Burst /
/// VelocityOverLifetime / Noise / Trail / SubEmitter / TextureSheetAnimation)
/// をカバーする。
ergo::gpu_particle::EmitterDescriptor ConvertToGpuEmitter(const ShurikenSource& src,
                                                          MigrationReport& report);

/// ShurikenNode ツリーを GpuEmitterNode の flatten 配列に変換する。
/// out[0] が root。 ツリー構造は parent_index で保持され、 各ノードの local TRS は
/// そのまま引き継がれる (ホスト側で world 行列に合成する想定)。
bool ConvertTreeToGpuEmitters(const ShurikenNode& root,
                              std::vector<GpuEmitterNode>& out,
                              MigrationReport& report);

/// EmitterDescriptor を JSON 文字列にシリアライズする (ホストが
/// 後で逆方向にロードして create_emitter する想定)。
std::string EmitterDescriptorToJson(const ergo::gpu_particle::EmitterDescriptor& desc);

/// 1 ファイルを読み、 含まれる全 ParticleSystem を 2D config の JSON に変換した
/// 配列文字列を返す。 失敗時は空文字列。
std::string MigrateFileToJson(const std::string& prefabPath,
                              MigrationReport& report);

/// 1 ファイルを読み、 gpu_particle::EmitterDescriptor の JSON 配列に変換する。
std::string MigrateFileToGpuJson(const std::string& prefabPath,
                                 MigrationReport& report);

/// 1 ファイルを読み、 子オブジェクト探索 + ツリー保持で全 emitter を変換し、
/// GpuEmitterNode 配列 (parent_index / local TRS 付き) の JSON にする。
std::string MigrateFileToGpuTreeJson(const std::string& prefabPath,
                                     MigrationReport& report);

}  // namespace ergo::shuriken_migrator
