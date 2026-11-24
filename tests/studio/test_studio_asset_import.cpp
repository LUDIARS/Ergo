#include "plugins/asset_import/asset_import_plugin.h"
#include "plugins/asset_import/capture_request.h"
#include "plugins/asset_import/preview_mesh.h"
#include "plugins/common/file_io.h"
#include "fixtures/asset_import_fbx.h"

#include "ergo/studio/ingress/remote_scope.h"
#include "pictor/visus/visus_serializer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <stdexcept>

namespace ergo::studio::plugins::asset_import {
namespace {
namespace fs = std::filesystem;

const std::string kFbx = studio_test::asset_fbx::triangle();

class AssetImport : public testing::Test {
protected:
    const fs::path root = fs::temp_directory_path() / ("ergo-visus-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const fs::path inbox = root / "import";

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    void write(const fs::path& path, const std::string& bytes) {
        std::string error;
        if (!write_text_file(path, bytes, error)) throw std::runtime_error(error);
    }
};

TEST_F(AssetImport, FbxAnalysis) {
    const auto analysis = analyze_fbx(kFbx);
    ASSERT_EQ(analysis.parts.size(), 1u);
    EXPECT_EQ(analysis.parts[0].part, "Paint");
    EXPECT_EQ(analysis.parts[0].metadata.get_string("texture.diffuse"), "paint.png");
    EXPECT_EQ(analysis.parts[0].metadata.get_string("material.shading_model"), "lambert");
    ASSERT_EQ(analysis.clips.size(), 1u);
    EXPECT_EQ(analysis.clips[0].as_string(), "Idle");
    EXPECT_EQ(analysis.maximum[0], 2);
    EXPECT_EQ(analysis.maximum[1], 3);
}

TEST_F(AssetImport, IncrementalCatalog) {
    write(inbox / "one" / "Hero.fbx", kFbx);
    write(inbox / "one" / "paint.png", "fixture");
    write(inbox / "two" / "Hero.fbx", kFbx);
    write(inbox / "broken.fbx", "broken");
    FolderAssetCatalog catalog(root / ".ergo-visus");
    int successful = 0;
    const Analyze analyze = [&](const auto&, const auto& bytes) {
        auto result = analyze_fbx(bytes);
        ++successful;
        return result;
    };
    const auto issues = import_assets(inbox, catalog, analyze);
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_FALSE(issues[0].reason.empty());
    ASSERT_EQ(catalog.entries().size(), 2u);
    const auto first = catalog.find("Hero");
    const auto bytes = read_bounded(first.visus_path(), kMaxVisusBytes);
    ASSERT_TRUE(bytes);
    pictor::VisusDesc desc;
    ASSERT_TRUE(pictor::from_visus_json(*bytes, desc));
    EXPECT_EQ(desc.name, "Hero");
    EXPECT_EQ(desc.kind, pictor::VisusKind::MODEL);
    ASSERT_EQ(desc.parts.size(), 1u);
    EXPECT_TRUE(desc.parts[0].shader.is_default_builtin());
    EXPECT_TRUE(desc.parts[0].metadata.has("texture.diffuse"));
    EXPECT_TRUE(desc.metadata.has("import.sha256"));
    EXPECT_FALSE(desc.metadata.has("scale.target_height"));
    EXPECT_EQ(catalog.entries()[1].name, "Hero_2");
    import_assets(inbox, catalog, analyze);
    EXPECT_EQ(successful, 2);
    write(inbox / "two" / "Hero.fbx", std::string(kFbx) + "\n; new source version");
    import_assets(inbox, catalog, analyze);
    EXPECT_EQ(successful, 3);
    const auto edited = *bytes + "\n";
    catalog.put(first, first.visus_filename(), edited);
    write(inbox / "one" / "Hero.fbx", std::string(kFbx) + "\n; changed");
    import_assets(inbox, catalog, analyze);
    EXPECT_EQ(successful, 3);
    EXPECT_EQ(read_bounded(first.visus_path(), kMaxVisusBytes), edited);

    const auto custom = catalog.find("Manual");
    const std::string manual = R"({"version":2,"name":"Manual","kind":"group"})";
    catalog.put(custom, custom.visus_filename(), manual);
    write(inbox / "Manual.fbx", kFbx);
    import_assets(inbox, catalog, analyze);
    EXPECT_EQ(read_bounded(custom.visus_path(), kMaxVisusBytes), manual);
    EXPECT_TRUE(fs::is_regular_file(catalog.find("Manual_2").visus_path()));
}

TEST_F(AssetImport, CaptureLifecycle) {
    const auto file = inbox / "Hero.fbx";
    write(file, kFbx);
    FolderAssetCatalog catalog(root / ".ergo-visus");
    ASSERT_TRUE(import_assets(inbox, catalog).empty());
    const auto entry = catalog.find("Hero");
    const auto definition = entry.visus_path();
    const auto bytes = read_bounded(definition, kMaxVisusBytes);
    ASSERT_TRUE(bytes);
    const auto revision = asset_revision(*bytes, file);
    std::string parse_error;
    const auto profile = parse_workspace_profile(R"({"import":"import"})", root / "profile.json", {}, parse_error);
    ASSERT_TRUE(profile);
    EXPECT_EQ(profile->import_dir, inbox);
    auto profiles = std::make_shared<FixedProfiles>(ProfileSet{{*profile}, {}});
    net::HttpRequest req;
    req.method = "POST";
    req.headers["host"] = "127.0.0.1";
    req.headers["content-type"] = "application/json";
    req.query["name"] = "Hero";
    {
        auto headless = make_asset_import_plugin(profiles, {}, {});
        const auto response = headless->http()->handle(req, "/api/capture");
        EXPECT_EQ(response.status, 200);
        EXPECT_NE(response.body.find("uncaptured"), std::string::npos);
        EXPECT_EQ(headless->http()->handle(req, "/api/import").status, 200);
        auto get = req;
        get.method = "GET";
        EXPECT_NE(headless->http()->handle(get, "/api/list").body.find("\"captured\":false"), std::string::npos);
    }
    std::optional<CaptureRequest> received;
    std::mutex received_mutex;
    {
        auto windowed = make_asset_import_plugin(profiles, [&](CaptureRequest request) {
            std::lock_guard<std::mutex> lock(received_mutex);
            received = std::move(request);
            return true;
        }, {});
        EXPECT_EQ(windowed->http()->handle(req, "/api/capture").status, 202);
        EXPECT_EQ(windowed->http()->handle(req, "/api/import").status, 200);
        {
            std::lock_guard<std::mutex> lock(received_mutex);
            ASSERT_TRUE(received);
            EXPECT_TRUE(store_capture(catalog, *received, "fake png"));
        }
        auto get = req;
        get.method = "GET";
        EXPECT_EQ(windowed->http()->handle(get, "/api/thumbnail").body, "fake png");
    }
    ASSERT_TRUE(received);
    EXPECT_EQ(received->revision, revision);
    EXPECT_EQ(read_capture(entry, revision), "fake png");
    EXPECT_FALSE(read_capture(entry, asset_revision(*bytes + "\n", file)));
    write(file, std::string(kFbx) + "\n; changed");
    EXPECT_FALSE(store_capture(catalog, *received, "stale png"));
    EXPECT_FALSE(read_capture(entry, asset_revision(*bytes, file)));
    EXPECT_TRUE(ingress::remote_refusal("POST", "/asset_import/api/capture"));
    EXPECT_FALSE(ingress::remote_refusal("GET", "/asset_import/api/list"));
}

TEST_F(AssetImport, SourceFamilies) {
    const auto fbx = inbox / "one" / "Hero.fbx";
    const auto obj = inbox / "one" / "Hero.OBJ";
    const auto blend = inbox / "one" / "Hero.blend";
    write(fbx, kFbx);
    write(obj, "alternative");
    write(inbox / "two" / "Hero.fbx", kFbx);
    write(inbox / "blend-only.BLEND", "source document");
    FolderAssetCatalog catalog(root / ".ergo-visus");
    std::vector<fs::path> selected;
    const Analyze analyze = [&](const auto& source, const auto& bytes) {
        selected.push_back(source);
        return analyze_model(source, bytes);
    };
    ASSERT_TRUE(import_assets(inbox, catalog, analyze).empty());
    ASSERT_EQ(selected.size(), 2u);
    EXPECT_EQ(selected.front(), fbx);
    ASSERT_EQ(catalog.entries().size(), 2u);
    const auto entry = catalog.find("Hero");
    auto json = read_bounded(entry.visus_path(), kMaxVisusBytes);
    ASSERT_TRUE(json);
    pictor::VisusDesc before;
    ASSERT_TRUE(pictor::from_visus_json(*json, before));
    const auto* sources = before.metadata.get_array("import.source_files");
    ASSERT_TRUE(sources);
    ASSERT_EQ(sources->size(), 1u);
    EXPECT_EQ(sources->front().as_string(), obj.generic_u8string());
    EXPECT_EQ(before.metadata.get_string("import.source"), fbx.generic_u8string());

    write(blend, "new alternate source");
    ASSERT_TRUE(import_assets(inbox, catalog, analyze).empty());
    EXPECT_EQ(selected.size(), 2u);
    json = read_bounded(entry.visus_path(), kMaxVisusBytes);
    ASSERT_TRUE(json);
    pictor::VisusDesc updated;
    ASSERT_TRUE(pictor::from_visus_json(*json, updated));
    EXPECT_EQ(updated.metadata.get_string("import.sha256"), before.metadata.get_string("import.sha256"));
    sources = updated.metadata.get_array("import.source_files");
    ASSERT_TRUE(sources);
    ASSERT_EQ(sources->size(), 2u);
    EXPECT_EQ(sources->at(0).as_string(), obj.generic_u8string());
    EXPECT_EQ(sources->at(1).as_string(), blend.generic_u8string());
    EXPECT_FALSE(preview_mesh(source_format(fbx), kFbx).empty());
}

TEST_F(AssetImport, SourcePriority) {
    const std::vector<std::string> suffixes{".OBJ", ".fbx", ".GLTF", ".glb", ".blend"};
    FolderAssetCatalog catalog(root / ".ergo-visus");
    std::vector<fs::path> selected;
    const Analyze analyze = [&](const auto& source, const auto&) {
        selected.push_back(source);
        return ModelAnalysis{};
    };
    for (std::size_t i = 0; i < suffixes.size(); ++i) {
        const auto source = inbox / ("Hero" + suffixes[i]);
        write(source, "same bytes");
        ASSERT_TRUE(import_assets(inbox, catalog, analyze).empty());
        ASSERT_EQ(selected.size(), std::min(i + 1, std::size_t{4}));
        if (i < 4) EXPECT_EQ(selected.back(), source);
        ASSERT_EQ(catalog.entries().size(), 1u);
    }
    const auto entry = catalog.find("Hero");
    const auto json = read_bounded(entry.visus_path(), kMaxVisusBytes);
    ASSERT_TRUE(json);
    pictor::VisusDesc desc;
    ASSERT_TRUE(pictor::from_visus_json(*json, desc));
    EXPECT_EQ((entry.directory / fs::u8path(desc.asset)).lexically_normal(), inbox / "Hero.glb");
    const auto* sources = desc.metadata.get_array("import.source_files");
    ASSERT_TRUE(sources);
    ASSERT_EQ(sources->size(), 4u);
    EXPECT_EQ(sources->front().as_string(), (inbox / "Hero.GLTF").generic_u8string());

    const auto edited = *json + "\n";
    catalog.put(entry, entry.visus_filename(), edited);
    fs::remove(inbox / "Hero.glb");
    EXPECT_TRUE(import_assets(inbox, catalog, analyze).empty());
    EXPECT_EQ(read_bounded(entry.visus_path(), kMaxVisusBytes), edited);
    EXPECT_EQ(selected.size(), 4u);
}

TEST_F(AssetImport, UnsupportedPictorLoaders) {
    const auto obj = inbox / "Wavefront.obj";
    write(obj, "mtllib Wavefront.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nusemtl Paint\nf 1 2 3\n");
    write(inbox / "Wavefront.mtl", "newmtl Paint\nKd 0 1 0\nmap_Kd paint.png\n");
    const auto gltf = inbox / "Scene.gltf";
    const std::string gltf_scene = R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],)"
        R"("nodes":[{"mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],)"
        R"("accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]}],)"
        R"("bufferViews":[{"buffer":0,"byteLength":36}],"buffers":[{"byteLength":36)";
    write(gltf, gltf_scene + R"(,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA"}]})");
    const auto glb = inbox / "Hero.glb";
    std::string glb_json = gltf_scene + "}]}";
    while (glb_json.size() % 4) glb_json += ' ';
    std::string glb_bytes = "glTF";
    const auto append_u32 = [&](std::uint32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8)
            glb_bytes += static_cast<char>((value >> shift) & 0xff);
    };
    append_u32(2);
    append_u32(static_cast<std::uint32_t>(28 + glb_json.size() + 36));
    append_u32(static_cast<std::uint32_t>(glb_json.size()));
    glb_bytes += "JSON";
    glb_bytes += glb_json;
    append_u32(36);
    append_u32(0x004e4942); // BIN chunk type
    for (unsigned axis = 0; axis < 9; ++axis) append_u32(axis == 3 || axis == 7 ? 0x3f800000 : 0);
    write(glb, glb_bytes);
    write(inbox / "Hero.fbx", kFbx);
    FolderAssetCatalog catalog(root / ".ergo-visus");
    const auto issues = import_assets(inbox, catalog);
    ASSERT_EQ(issues.size(), 3u);
    for (const auto& issue : issues) {
        EXPECT_NE(issue.reason.find("Pictor"), std::string::npos);
        EXPECT_NE(issue.reason.find("loader"), std::string::npos);
    }
    EXPECT_EQ(issues.front().source, glb.u8string());
    EXPECT_TRUE(catalog.entries().empty());
    for (const auto& file : {obj, gltf, glb}) {
        const auto bytes = read_bounded(file, kMaxModelBytes);
        ASSERT_TRUE(bytes);
        std::string reason;
        try {
            preview_mesh(source_format(file), *bytes);
        } catch (const std::runtime_error& error) {
            reason = error.what();
        }
        EXPECT_NE(reason.find("Pictor"), std::string::npos);
    }
}

} // namespace
} // namespace ergo::studio::plugins::asset_import
