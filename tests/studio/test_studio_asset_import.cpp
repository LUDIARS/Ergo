#include "plugins/asset_import/asset_import_plugin.h"
#include "plugins/asset_import/capture_request.h"
#include "plugins/common/file_io.h"
#include "fixtures/asset_import_fbx.h"

#include "ergo/studio/ingress/remote_scope.h"
#include "pictor/visus/visus_serializer.h"

#include <gtest/gtest.h>

#include <chrono>
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
    const Analyze analyze = [&](const auto& bytes) {
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

} // namespace
} // namespace ergo::studio::plugins::asset_import
