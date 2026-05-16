#include "ergo/shuriken_migrator/migrator.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace ergo::shuriken_migrator {

void MinMaxCurve::Range(float& outMin, float& outMax) const {
    switch (state) {
        case MinMaxState::Constant:
            outMin = outMax = scalar * scalarMultiplier;
            return;
        case MinMaxState::TwoConstants:
            outMin = minScalar * scalarMultiplier;
            outMax = maxScalar * scalarMultiplier;
            return;
        case MinMaxState::Curve:
        case MinMaxState::TwoCurves:
            // Curve サンプリングは v1 で未対応。 scalar (= curve multiplier) を min=max として近似。
            outMin = outMax = scalar * scalarMultiplier;
            return;
    }
    outMin = outMax = 0.0f;
}

namespace {

// -------- 文字列ユーティリティ --------

std::string Trim(std::string_view s) {
    size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return std::string(s.substr(a, b - a));
}

int Indent(std::string_view line) {
    int n = 0;
    for (char c : line) {
        if (c == ' ') ++n;
        else break;
    }
    return n;
}

bool ParseFloat(const std::string& s, float& out) {
    try { out = std::stof(s); return true; } catch (...) { return false; }
}

bool ParseInt(const std::string& s, int& out) {
    try { out = std::stoi(s); return true; } catch (...) { return false; }
}

// -------- ドキュメントブロック抽出 --------

/// Unity YAML の 1 ドキュメントブロック。
///   `--- !u!<classId> &<anchor> [stripped]`
struct YamlBlock {
    int         classId  = -1;
    long long   anchor   = 0;
    bool        stripped = false;
    std::string body;
};

/// "--- !u!<id> &<n>" 行から始まるブロックを全て切り出す。
std::vector<YamlBlock> SplitBlocks(const std::string& yaml) {
    std::vector<YamlBlock> blocks;
    std::istringstream is(yaml);
    std::string line;
    YamlBlock cur;
    bool have = false;
    auto flush = [&]() {
        if (have) blocks.push_back(std::move(cur));
        cur = YamlBlock{};
        have = false;
    };
    while (std::getline(is, line)) {
        if (line.size() >= 6 && line.compare(0, 6, "--- !u") == 0) {
            flush();
            have = true;
            // "--- !u!<id> &<num> [stripped]" を緩くパース。
            size_t cls = line.find("!u!");
            if (cls != std::string::npos) {
                size_t s = cls + 3;
                size_t sp = line.find_first_of(" \t&", s);
                ParseInt(line.substr(s, (sp == std::string::npos ? line.size() : sp) - s),
                         cur.classId);
            }
            size_t amp = line.find('&');
            if (amp != std::string::npos) {
                size_t s = amp + 1;
                size_t e = s;
                while (e < line.size() && std::isdigit(static_cast<unsigned char>(line[e]))) ++e;
                try { cur.anchor = std::stoll(line.substr(s, e - s)); } catch (...) {}
            }
            cur.stripped = line.find("stripped") != std::string::npos;
            continue;
        }
        if (have) { cur.body += line; cur.body += '\n'; }
    }
    flush();
    return blocks;
}

/// インラインマップ `{fileID: 123}` / `{x: 1, y: 2, z: 3}` から `key` の値文字列を返す。
/// `key` 単体の境界 ( '{' / ' ' / ',' の直後、 直後が ':') を確認して部分一致を避ける。
bool InlineRaw(const std::string& braced, const char* key, std::string& out) {
    const std::string pat = key;
    size_t k = braced.find(pat);
    while (k != std::string::npos) {
        const bool boundaryL = (k == 0) || braced[k - 1] == '{' ||
                               braced[k - 1] == ' ' || braced[k - 1] == ',';
        size_t c = k + pat.size();
        while (c < braced.size() && braced[c] == ' ') ++c;
        if (boundaryL && c < braced.size() && braced[c] == ':') {
            size_t v = c + 1;
            while (v < braced.size() && braced[v] == ' ') ++v;
            size_t e = v;
            while (e < braced.size() && braced[e] != ',' && braced[e] != '}') ++e;
            out = braced.substr(v, e - v);
            return true;
        }
        k = braced.find(pat, k + 1);
    }
    return false;
}

/// `{x: 1.5, ...}` から float を読む。
bool InlineNumber(const std::string& braced, const char* key, double& out) {
    std::string raw;
    if (!InlineRaw(braced, key, raw)) return false;
    try { out = std::stod(raw); return true; } catch (...) { return false; }
}

/// `{fileID: 1582022730285666577}` から 64bit 整数を読む。 fileID は 18〜19 桁
/// あり double では精度が落ちるので必ず整数としてパースする。
bool InlineInt64(const std::string& braced, const char* key, long long& out) {
    std::string raw;
    if (!InlineRaw(braced, key, raw)) return false;
    try { out = std::stoll(raw); return true; } catch (...) { return false; }
}

// -------- key:value マップ展開 (path-flat) --------

/// 1 ブロックの YAML 本文から、 階層パス (例 `InitialModule.startLifetime.scalar`)
/// → 文字列値 のマップを作る。 シンプルな indent 木。
std::unordered_map<std::string, std::string> FlattenBlock(const std::string& body) {
    std::unordered_map<std::string, std::string> out;
    std::istringstream is(body);
    std::string raw;

    // path stack: (indent, key)
    std::vector<std::pair<int, std::string>> stack;

    while (std::getline(is, raw)) {
        if (raw.empty()) continue;
        if (raw.size() >= 1 && raw[0] == '#') continue;

        int ind = Indent(raw);
        std::string content = Trim(std::string_view(raw).substr(ind));
        if (content.empty()) continue;
        if (content.front() == '-' && content.size() > 1 && content[1] == ' ') {
            // list item — v1 ではサポート外 (bursts など). スキップ。
            continue;
        }

        // pop until stack top has indent < ind
        while (!stack.empty() && stack.back().first >= ind) stack.pop_back();

        // "key: value" or "key:"
        size_t colon = content.find(':');
        if (colon == std::string::npos) continue;
        std::string key = Trim(content.substr(0, colon));
        std::string val = (colon + 1 < content.size()) ? Trim(content.substr(colon + 1)) : "";

        // build path
        std::string path;
        for (auto& s : stack) { path += s.second; path += '.'; }
        path += key;

        if (!val.empty()) {
            out[path] = val;
        } else {
            stack.emplace_back(ind, key);
        }
    }
    return out;
}

// -------- 個別フィールド抽出 helpers --------

const std::string* Get(const std::unordered_map<std::string, std::string>& m, const std::string& key) {
    auto it = m.find(key);
    return (it == m.end()) ? nullptr : &it->second;
}

void TryFloat(const std::unordered_map<std::string, std::string>& m, const std::string& key, float& dst) {
    if (auto* p = Get(m, key)) ParseFloat(*p, dst);
}

void TryInt(const std::unordered_map<std::string, std::string>& m, const std::string& key, int& dst) {
    if (auto* p = Get(m, key)) ParseInt(*p, dst);
}

void TryBool(const std::unordered_map<std::string, std::string>& m, const std::string& key, bool& dst) {
    if (auto* p = Get(m, key)) {
        int v = 0;
        if (ParseInt(*p, v)) dst = (v != 0);
    }
}

void ReadMinMaxCurve(const std::unordered_map<std::string, std::string>& m,
                     const std::string& prefix, MinMaxCurve& dst) {
    int st = 0;
    TryInt(m, prefix + ".minMaxState", st);
    dst.state = static_cast<MinMaxState>(st);
    TryFloat(m, prefix + ".scalar", dst.scalar);
    TryFloat(m, prefix + ".minScalar", dst.minScalar);
    TryFloat(m, prefix + ".maxScalar", dst.maxScalar);
    TryFloat(m, prefix + ".scalarMultiplier", dst.scalarMultiplier);
    if (dst.scalarMultiplier == 0.0f) dst.scalarMultiplier = 1.0f;
}

void ReadColor4(const std::unordered_map<std::string, std::string>& m,
                const std::string& prefix, float (&dst)[4]) {
    TryFloat(m, prefix + ".r", dst[0]);
    TryFloat(m, prefix + ".g", dst[1]);
    TryFloat(m, prefix + ".b", dst[2]);
    TryFloat(m, prefix + ".a", dst[3]);
}

}  // namespace

// -------- public ----------

namespace {

/// GameObject ブロックの body から m_Name を読む。
std::string ReadGameObjectName(const std::string& body) {
    std::istringstream is(body);
    std::string line;
    while (std::getline(is, line)) {
        std::string t = Trim(line);
        if (t.rfind("m_Name:", 0) == 0) {
            std::string n = Trim(t.substr(7));
            if (n.size() >= 2 && n.front() == '"' && n.back() == '"')
                n = n.substr(1, n.size() - 2);
            return n;
        }
    }
    return {};
}

/// ParticleSystem (file ID 198) ブロックの body から ShurikenSource を組み立てる。
/// name は呼び出し側 (GameObject 名) が設定する。
ShurikenSource ParseShurikenFromBody(const std::string& body, MigrationReport& report) {
    ShurikenSource src;
    auto map = FlattenBlock(body);

    // Main
    TryFloat(map, "ParticleSystem.lengthInSec", src.main.duration);
        TryBool(map,  "ParticleSystem.looping",     src.main.looping);
        TryInt(map,   "ParticleSystem.simulationSpace", src.main.simulationSpace);
        TryInt(map,   "ParticleSystem.maxNumParticles", src.main.maxNumParticles);

        ReadMinMaxCurve(map, "ParticleSystem.InitialModule.startLifetime", src.main.startLifetime);
        ReadMinMaxCurve(map, "ParticleSystem.InitialModule.startSpeed",    src.main.startSpeed);
        ReadMinMaxCurve(map, "ParticleSystem.InitialModule.startSize",     src.main.startSize);
        ReadMinMaxCurve(map, "ParticleSystem.InitialModule.gravityModifier", src.main.gravityModifier);
        ReadColor4(map, "ParticleSystem.InitialModule.startColor.maxColor", src.main.startColor);
        // 一部の prefab では maxColor ではなく直接 r/g/b/a が居る — フォールバック
        if (src.main.startColor[3] == 1.0f && src.main.startColor[0] == 1.0f) {
            ReadColor4(map, "ParticleSystem.InitialModule.startColor", src.main.startColor);
        }

        // Emission
        TryBool(map, "ParticleSystem.EmissionModule.enabled", src.emission.enabled);
        ReadMinMaxCurve(map, "ParticleSystem.EmissionModule.rateOverTime", src.emission.rateOverTime);
        if (Get(map, "ParticleSystem.EmissionModule.m_Bursts")) {
            report.Warn(src.name + ": Emission bursts は v1 で未対応");
        }

        // Shape
        TryBool(map,  "ParticleSystem.ShapeModule.enabled",   src.shape.enabled);
        TryInt(map,   "ParticleSystem.ShapeModule.type",      src.shape.shapeType);
        TryFloat(map, "ParticleSystem.ShapeModule.radius",    src.shape.radius);
        TryFloat(map, "ParticleSystem.ShapeModule.angle",     src.shape.angle);
        TryFloat(map, "ParticleSystem.ShapeModule.m_Scale.x", src.shape.scale[0]);
        TryFloat(map, "ParticleSystem.ShapeModule.m_Scale.y", src.shape.scale[1]);
        TryFloat(map, "ParticleSystem.ShapeModule.m_Scale.z", src.shape.scale[2]);

        // ColorOverLifetime
        TryBool(map, "ParticleSystem.ColorModule.enabled", src.colorOverLifetime.enabled);
        if (src.colorOverLifetime.enabled) {
            // Gradient minMaxState
            int gst = 0;
            TryInt(map, "ParticleSystem.ColorModule.gradient.minMaxState", gst);
            src.colorOverLifetime.gradient.state = static_cast<MinMaxState>(gst);
            // 完全な curve は v1 でサンプリングしない
            report.Warn(src.name + ": ColorOverLifetime gradient は v1 では start/end 推定のみ");
        }

        // SizeOverLifetime
        TryBool(map, "ParticleSystem.SizeModule.enabled", src.sizeOverLifetime.enabled);
        if (src.sizeOverLifetime.enabled) {
            ReadMinMaxCurve(map, "ParticleSystem.SizeModule.size", src.sizeOverLifetime.size);
        }

        // VelocityOverLifetime
        TryBool(map, "ParticleSystem.VelocityModule.enabled", src.velocityOverLifetime.enabled);
        if (src.velocityOverLifetime.enabled) {
            ReadMinMaxCurve(map, "ParticleSystem.VelocityModule.x", src.velocityOverLifetime.x);
            ReadMinMaxCurve(map, "ParticleSystem.VelocityModule.y", src.velocityOverLifetime.y);
            ReadMinMaxCurve(map, "ParticleSystem.VelocityModule.z", src.velocityOverLifetime.z);
        }

        // LimitVelocityOverLifetime (Unity 名: ClampVelocityModule)
        TryBool(map, "ParticleSystem.ClampVelocityModule.enabled", src.limitVelocityOverLifetime.enabled);
        if (src.limitVelocityOverLifetime.enabled) {
            ReadMinMaxCurve(map, "ParticleSystem.ClampVelocityModule.magnitude",
                            src.limitVelocityOverLifetime.speed);
            TryFloat(map, "ParticleSystem.ClampVelocityModule.dampen",
                     src.limitVelocityOverLifetime.dampen);
            TryFloat(map, "ParticleSystem.ClampVelocityModule.drag.scalar",
                     src.limitVelocityOverLifetime.drag);
        }

        // Noise
        TryBool(map, "ParticleSystem.NoiseModule.enabled", src.noise.enabled);
        if (src.noise.enabled) {
            TryFloat(map, "ParticleSystem.NoiseModule.strength.scalar",  src.noise.strength);
            TryFloat(map, "ParticleSystem.NoiseModule.frequency",        src.noise.frequency);
            TryInt(map,   "ParticleSystem.NoiseModule.octaveCount",      src.noise.octaves);
            TryFloat(map, "ParticleSystem.NoiseModule.octaveMultiplier", src.noise.octave_multiplier);
            TryFloat(map, "ParticleSystem.NoiseModule.octaveScale",      src.noise.octave_scale);
            TryBool(map,  "ParticleSystem.NoiseModule.separateAxes",     src.noise.separate_axes);
            TryFloat(map, "ParticleSystem.NoiseModule.scrollSpeed.scalar", src.noise.scroll_speed);
        }

        // Trails
        TryBool(map, "ParticleSystem.TrailModule.enabled", src.trails.enabled);
        if (src.trails.enabled) {
            float lo, hi;
            MinMaxCurve life; ReadMinMaxCurve(map, "ParticleSystem.TrailModule.lifetime", life);
            life.Range(lo, hi); src.trails.lifetime = (lo + hi) * 0.5f;
            MinMaxCurve width; ReadMinMaxCurve(map, "ParticleSystem.TrailModule.widthOverTrail", width);
            width.Range(lo, hi); src.trails.width = (lo + hi) * 0.5f;
            TryFloat(map, "ParticleSystem.TrailModule.minVertexDistance",
                     src.trails.min_vertex_distance);
            TryBool(map, "ParticleSystem.TrailModule.inheritParticleColor",
                    src.trails.inherit_particle_color);
            TryBool(map, "ParticleSystem.TrailModule.dieWithParticles",
                    src.trails.die_with_particle);
            ReadColor4(map, "ParticleSystem.TrailModule.colorOverTrail.minColor", src.trails.color_start);
            ReadColor4(map, "ParticleSystem.TrailModule.colorOverTrail.maxColor", src.trails.color_end);
        }

        // SubEmitters (Unity: ParticleSystem.SubModule.subEmitters は array)
        // SubModule の配列フラット化は FlattenBlock では一意 path にならない (配列要素は
        // 0/1/2 等の index で複製される)。 v1 では「sub emitter が存在するか」だけ拾い、
        // emitter_index は 0 固定 (ホスト側で解決)。
        bool sub_enabled = false;
        TryBool(map, "ParticleSystem.SubModule.enabled", sub_enabled);
        if (sub_enabled && Get(map, "ParticleSystem.SubModule.subEmitters")) {
            ShurikenSubEmitter se;
            se.event = 1;          // Death を既定
            se.emitter_index = 0;
            se.probability = 1.0f;
            src.subEmitters.push_back(se);
            report.Warn(src.name + ": SubModule 配列パースは v1 で簡略化 (event=Death/index=0)");
        }

        // TextureSheetAnimation (Unity: UVModule)
        TryBool(map, "ParticleSystem.UVModule.enabled", src.textureSheet.enabled);
        if (src.textureSheet.enabled) {
            TryInt(map,   "ParticleSystem.UVModule.tilesX",        src.textureSheet.num_tiles_x);
            TryInt(map,   "ParticleSystem.UVModule.tilesY",        src.textureSheet.num_tiles_y);
            TryInt(map,   "ParticleSystem.UVModule.animationType", src.textureSheet.time_mode);
            TryFloat(map, "ParticleSystem.UVModule.fps",           src.textureSheet.fps);
            TryInt(map,   "ParticleSystem.UVModule.cycles",        src.textureSheet.cycles);
            TryBool(map,  "ParticleSystem.UVModule.useRandomRow",  src.textureSheet.random_row);
        }

        // Bursts (Emission.m_Bursts) — array で表現される。 簡易対応として「最低 1 burst」のみ拾う
        if (Get(map, "ParticleSystem.EmissionModule.m_Bursts")) {
            report.Warn(src.name + ": Emission bursts はパース簡易対応 (詳細は ConvertToGpu で扱う)");
        }

        // ForceModule / RotationModule は EmitterDescriptor の対応フィールド無し
        if (Get(map, "ParticleSystem.ForceModule"))    report.Unsupp("forceOverLifetime");
        if (Get(map, "ParticleSystem.RotationModule")) report.Unsupp("rotationOverLifetime");

    return src;
}

/// Transform (file ID 4) ブロックの body から親子・local TRS を読む。
struct TransformInfo {
    long long         gameObject = 0;
    long long         father     = 0;
    ShurikenTransform trs;
};
TransformInfo ParseTransformFromBody(const std::string& body) {
    TransformInfo ti;
    auto map = FlattenBlock(body);
    double d = 0.0;
    long long id = 0;
    if (auto* p = Get(map, "Transform.m_GameObject"))
        if (InlineInt64(*p, "fileID", id)) ti.gameObject = id;
    if (auto* p = Get(map, "Transform.m_Father"))
        if (InlineInt64(*p, "fileID", id)) ti.father = id;
    if (auto* p = Get(map, "Transform.m_LocalPosition")) {
        if (InlineNumber(*p, "x", d)) ti.trs.local_position[0] = static_cast<float>(d);
        if (InlineNumber(*p, "y", d)) ti.trs.local_position[1] = static_cast<float>(d);
        if (InlineNumber(*p, "z", d)) ti.trs.local_position[2] = static_cast<float>(d);
    }
    if (auto* p = Get(map, "Transform.m_LocalRotation")) {
        if (InlineNumber(*p, "x", d)) ti.trs.local_rotation[0] = static_cast<float>(d);
        if (InlineNumber(*p, "y", d)) ti.trs.local_rotation[1] = static_cast<float>(d);
        if (InlineNumber(*p, "z", d)) ti.trs.local_rotation[2] = static_cast<float>(d);
        if (InlineNumber(*p, "w", d)) ti.trs.local_rotation[3] = static_cast<float>(d);
    }
    if (auto* p = Get(map, "Transform.m_LocalScale")) {
        if (InlineNumber(*p, "x", d)) ti.trs.local_scale[0] = static_cast<float>(d);
        if (InlineNumber(*p, "y", d)) ti.trs.local_scale[1] = static_cast<float>(d);
        if (InlineNumber(*p, "z", d)) ti.trs.local_scale[2] = static_cast<float>(d);
    }
    return ti;
}

}  // namespace

bool ParsePrefabYaml(const std::string& yamlText,
                     std::vector<ShurikenSource>& outSystems,
                     MigrationReport& report) {
    auto blocks = SplitBlocks(yamlText);

    // component リストの参照は深追いせず、 「直前に出てきた GameObject 名」を
    // ParticleSystem のラベルとして拾う best-effort (flat 抽出の従来挙動)。
    std::string lastName = "particle";
    for (const auto& blk : blocks) {
        if (blk.classId == 1) {
            std::string n = ReadGameObjectName(blk.body);
            if (!n.empty()) lastName = n;
        }
        if (blk.classId != 198 || blk.stripped) continue;
        ShurikenSource src = ParseShurikenFromBody(blk.body, report);
        src.name = lastName;
        outSystems.push_back(std::move(src));
        report.extractedSystems++;
    }
    return true;
}

bool ParsePrefabTree(const std::string& yamlText,
                     ShurikenNode& outRoot,
                     MigrationReport& report) {
    auto blocks = SplitBlocks(yamlText);

    std::unordered_map<long long, std::string>   goName;       // GO anchor → 名前
    std::unordered_map<long long, TransformInfo> transforms;   // Transform anchor → info
    std::unordered_map<long long, ShurikenSource> goEmitter;   // GO anchor → emitter
    std::vector<long long> goOrder;                            // GO 発見順
    bool sawStripped = false;

    for (const auto& blk : blocks) {
        if (blk.stripped) {
            if (blk.classId == 198 || blk.classId == 4 || blk.classId == 1) sawStripped = true;
            continue;
        }
        switch (blk.classId) {
            case 1: {  // GameObject
                std::string n = ReadGameObjectName(blk.body);
                if (goName.find(blk.anchor) == goName.end()) goOrder.push_back(blk.anchor);
                goName[blk.anchor] = n.empty() ? "GameObject" : n;
                break;
            }
            case 4: {  // Transform
                transforms[blk.anchor] = ParseTransformFromBody(blk.body);
                break;
            }
            case 198: {  // ParticleSystem
                auto map = FlattenBlock(blk.body);
                long long go = 0;
                if (auto* p = Get(map, "ParticleSystem.m_GameObject"))
                    InlineInt64(*p, "fileID", go);
                ShurikenSource src = ParseShurikenFromBody(blk.body, report);
                if (go != 0) {
                    if (goEmitter.find(go) != goEmitter.end())
                        report.Warn("GameObject " + std::to_string(go) +
                                    ": 複数 ParticleSystem — 先頭のみ採用");
                    else
                        goEmitter.emplace(go, std::move(src));
                }
                report.extractedSystems++;
                break;
            }
            default: break;
        }
    }

    if (sawStripped) {
        report.Warn("prefab-instance 経由の stripped component を検出 — "
                    "元 prefab の展開は未対応 (該当ノードは空グループになる)");
    }

    // GO anchor → その GameObject の Transform anchor。
    std::unordered_map<long long, long long> goToTransform;
    for (const auto& [trAnchor, ti] : transforms)
        if (ti.gameObject != 0) goToTransform[ti.gameObject] = trAnchor;

    // GO anchor → 親 GO anchor (0 = ルート)。 + 親 → 子リスト (発見順)。
    std::unordered_map<long long, long long>              parentGo;
    std::unordered_map<long long, std::vector<long long>> childrenGo;
    std::vector<long long> roots;
    for (long long go : goOrder) {
        long long parent = 0;
        auto trIt = goToTransform.find(go);
        if (trIt != goToTransform.end()) {
            long long father = transforms[trIt->second].father;
            if (father != 0) {
                auto fIt = transforms.find(father);
                if (fIt != transforms.end()) parent = fIt->second.gameObject;
            }
        }
        parentGo[go] = parent;
        if (parent == 0) roots.push_back(go);
        else             childrenGo[parent].push_back(go);
    }

    // GO anchor → ShurikenNode を再帰構築する。
    std::function<ShurikenNode(long long)> build = [&](long long go) -> ShurikenNode {
        ShurikenNode n;
        auto nameIt = goName.find(go);
        n.name = (nameIt != goName.end()) ? nameIt->second : "GameObject";
        auto trIt = goToTransform.find(go);
        if (trIt != goToTransform.end()) n.transform = transforms[trIt->second].trs;
        auto emIt = goEmitter.find(go);
        if (emIt != goEmitter.end()) {
            n.has_emitter = true;
            n.emitter = emIt->second;
            n.emitter.name = n.name;
        }
        auto chIt = childrenGo.find(go);
        if (chIt != childrenGo.end())
            for (long long c : chIt->second) n.children.push_back(build(c));
        return n;
    };

    if (roots.size() == 1) {
        outRoot = build(roots[0]);
    } else if (roots.size() > 1) {
        // ルートが複数 — 合成ルートでまとめる。
        outRoot = ShurikenNode{};
        outRoot.name = "(prefab root)";
        for (long long r : roots) outRoot.children.push_back(build(r));
    } else {
        // Transform 階層を復元できなかった — emitter を flat にぶら下げる。
        report.Warn("Transform 階層を復元できず — emitter をフラット展開");
        outRoot = ShurikenNode{};
        outRoot.name = "(prefab root)";
        for (auto& [go, src] : goEmitter) {
            ShurikenNode n;
            auto nameIt = goName.find(go);
            n.name = (nameIt != goName.end()) ? nameIt->second : "GameObject";
            n.has_emitter = true;
            n.emitter = src;
            n.emitter.name = n.name;
            outRoot.children.push_back(std::move(n));
        }
    }
    return true;
}

ergo::particle::ParticleEffectConfig ConvertToErgo(const ShurikenSource& src, MigrationReport& report) {
    ergo::particle::ParticleEffectConfig out;
    out.name = src.name.empty() ? std::string("untitled") : src.name;

    // Lifetime
    src.main.startLifetime.Range(out.init_lifetime_min, out.init_lifetime_max);
    if (out.init_lifetime_min < 0.01f) out.init_lifetime_min = 0.01f;
    if (out.init_lifetime_max < out.init_lifetime_min) out.init_lifetime_max = out.init_lifetime_min;

    // Speed
    src.main.startSpeed.Range(out.init_speed_min, out.init_speed_max);

    // Size — 最小と最大の平均をベースサイズに採用
    float sMin, sMax;
    src.main.startSize.Range(sMin, sMax);
    out.init_size = (sMin + sMax) * 0.5f;
    if (out.init_size <= 0.0f) out.init_size = 1.0f;

    // Color
    out.init_color = { src.main.startColor[0], src.main.startColor[1],
                       src.main.startColor[2], src.main.startColor[3] };
    out.life_color_start = out.init_color;
    out.life_color_end   = { out.init_color[0], out.init_color[1], out.init_color[2], 0.0f };

    // Gravity (Unity gravityModifier は重力定数 9.81 倍)
    float gMin, gMax;
    src.main.gravityModifier.Range(gMin, gMax);
    float gAvg = (gMin + gMax) * 0.5f;
    out.gravity = { 0.0f, gAvg * 9.81f };  // 2D gravity: +y は下向き

    // Emission rate
    float eMin, eMax;
    src.emission.rateOverTime.Range(eMin, eMax);
    out.emission_rate = (eMin + eMax) * 0.5f;
    if (out.emission_rate <= 0.0f) out.emission_rate = 1.0f;

    out.emission_max_alive = std::min(src.main.maxNumParticles, 4000);
    if (out.emission_max_alive < 1) out.emission_max_alive = 400;

    // Shape — 半径と角度
    out.init_position_radius = src.shape.radius;
    if (src.shape.shapeType == 5 /* Cone */) {
        out.init_velocity_spread_deg = src.shape.angle;
    } else {
        // 球/円 系は全方位に近い
        out.init_velocity_spread_deg = 360.0f;
    }
    out.init_velocity_angle_deg = 270.0f;  // -y 既定 (Unity 同様 cone 系は y- へ向く想定)

    // Size over lifetime (粗いサンプリング: scalar を end として採用)
    if (src.sizeOverLifetime.enabled) {
        float lMin, lMax;
        src.sizeOverLifetime.size.Range(lMin, lMax);
        out.life_size_start = 1.0f;
        out.life_size_end   = std::max(0.0f, (lMin + lMax) * 0.5f);
    } else {
        out.life_size_start = 1.0f;
        out.life_size_end   = 0.0f;
    }

    // 速度減衰 — v1 では default 0.6 を維持. velocityOverLifetime damping を真面目に拾うのは v2.
    out.life_velocity_damping = 0.6f;

    // Render — Shuriken の billboard / mesh 区別は v1 ではしない
    out.render_blend = ergo::particle::BlendMode::Additive;
    out.render_shape = ergo::particle::ShapeMode::Circle;

    if (!src.shape.enabled) {
        report.Warn(src.name + ": Shape disabled — radius を 0 にせず Unity 既定 1.0 を使う");
    }

    return out;
}

namespace {

void EscapeJson(const std::string& in, std::string& out) {
    for (char c : in) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
}

void WriteFloat(std::string& out, float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6g", v);
    out += buf;
}

void WriteColor(std::string& out, const std::array<float, 4>& c) {
    out += "[";
    for (int i = 0; i < 4; ++i) {
        if (i) out += ",";
        WriteFloat(out, c[i]);
    }
    out += "]";
}

std::string SerializeConfigJson(const ergo::particle::ParticleEffectConfig& c) {
    std::string s;
    s += "{";
    s += "\"version\":";              s += std::to_string(c.version);
    s += ",\"name\":\"";              EscapeJson(c.name, s); s += "\"";
    s += ",\"emission_rate\":";       WriteFloat(s, c.emission_rate);
    s += ",\"emission_max_alive\":";  s += std::to_string(c.emission_max_alive);
    s += ",\"init_position_radius\":";    WriteFloat(s, c.init_position_radius);
    s += ",\"init_velocity_angle_deg\":"; WriteFloat(s, c.init_velocity_angle_deg);
    s += ",\"init_velocity_spread_deg\":";WriteFloat(s, c.init_velocity_spread_deg);
    s += ",\"init_speed_min\":";          WriteFloat(s, c.init_speed_min);
    s += ",\"init_speed_max\":";          WriteFloat(s, c.init_speed_max);
    s += ",\"init_lifetime_min\":";       WriteFloat(s, c.init_lifetime_min);
    s += ",\"init_lifetime_max\":";       WriteFloat(s, c.init_lifetime_max);
    s += ",\"init_size\":";               WriteFloat(s, c.init_size);
    s += ",\"init_color\":";              WriteColor(s, c.init_color);
    s += ",\"life_size_start\":";         WriteFloat(s, c.life_size_start);
    s += ",\"life_size_end\":";           WriteFloat(s, c.life_size_end);
    s += ",\"life_color_start\":";        WriteColor(s, c.life_color_start);
    s += ",\"life_color_end\":";          WriteColor(s, c.life_color_end);
    s += ",\"life_velocity_damping\":";   WriteFloat(s, c.life_velocity_damping);
    s += ",\"gravity\":[";                WriteFloat(s, c.gravity[0]); s += ","; WriteFloat(s, c.gravity[1]); s += "]";
    s += ",\"render_blend\":\""; s += (c.render_blend == ergo::particle::BlendMode::Additive ? "additive" : "alpha"); s += "\"";
    s += ",\"render_shape\":\""; s += (c.render_shape == ergo::particle::ShapeMode::Circle ? "circle" : "square"); s += "\"";
    s += "}";
    return s;
}

}  // namespace

std::string MigrateFileToJson(const std::string& prefabPath, MigrationReport& report) {
    std::ifstream f(prefabPath);
    if (!f) {
        report.Warn("file open failed: " + prefabPath);
        return "";
    }
    std::stringstream buf;
    buf << f.rdbuf();
    std::string yaml = buf.str();

    std::vector<ShurikenSource> systems;
    if (!ParsePrefabYaml(yaml, systems, report)) {
        return "";
    }

    std::string out = "[";
    for (size_t i = 0; i < systems.size(); ++i) {
        if (i) out += ",";
        auto cfg = ConvertToErgo(systems[i], report);
        out += SerializeConfigJson(cfg);
        report.convertedSystems++;
    }
    out += "]";
    return out;
}

}  // namespace ergo::shuriken_migrator
