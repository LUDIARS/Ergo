// update --to の解決 (spec/feature/package-manager.md §2.2 / §7 W2): タグは expect_tag つき、 main と SHA は
// STRICT の警告、 feature ブランチ / main の祖先でない SHA は拒否し --allow-off-main で通る。 git は偽物。

#include "ergo/package_manager/rev_resolution.h"

#include "gtest/gtest.h"

#include <map>
#include <set>
#include <string>
#include <utility>

using namespace ergo::package_manager;

namespace {

const std::string kRoot    = "1000000000000000000000000000000000000000";
const std::string kTagged  = "2000000000000000000000000000000000000000";
const std::string kMain    = "3000000000000000000000000000000000000000";
const std::string kFeature = "4000000000000000000000000000000000000000";

/// main: kRoot → kTagged (v1.0.0) → kMain。 feat/x: kTagged → kFeature (main の祖先でない)。
class FakeRevs : public IRevResolver {
public:
    bool online = true;

    bool tag_commit(const std::string& tag, bool& found, std::string& sha) override {
        return lookup(tags_, tag, found, sha);
    }
    bool branch_head(const std::string& branch, bool& found, std::string& sha) override {
        return lookup(branches_, branch, found, sha);
    }
    bool has_commit(const std::string& sha, bool& found) override {
        found = ancestors_.count(sha) != 0;
        return online;
    }
    bool is_ancestor(const std::string& a, const std::string& d, bool& yes) override {
        const auto it = ancestors_.find(d);
        yes = it != ancestors_.end() && it->second.count(a) != 0;
        return online;
    }

private:
    bool lookup(const std::map<std::string, std::string>& refs, const std::string& name, bool& found,
                std::string& sha) {
        const auto it = refs.find(name);
        found = it != refs.end();
        if (found) sha = it->second;
        return online;
    }

    std::map<std::string, std::string> tags_     = {{"v1.0.0", kTagged}, {"v9.9.9-feature", kFeature}};
    std::map<std::string, std::string> branches_ = {{"main", kMain}, {"feat/x", kFeature}};
    /// commit → その祖先 (自分を含む)
    std::map<std::string, std::set<std::string>> ancestors_ = {
        {kRoot, {kRoot}},
        {kTagged, {kRoot, kTagged}},
        {kMain, {kRoot, kTagged, kMain}},
        {kFeature, {kRoot, kTagged, kFeature}},
    };
};

RevResolution resolve(const std::string& to, bool allow_off_main = false) {
    FakeRevs revs;
    RevRequest request;
    request.to             = to;
    request.allow_off_main = allow_off_main;
    return resolve_rev(request, revs);
}

} // namespace

TEST(PackageManagerRevResolution, TagGivesShaAndExpectTag) {
    const auto r = resolve("v1.0.0");
    ASSERT_TRUE(r.status == ResolveStatus::Ok);
    EXPECT_EQ(r.rev, kTagged);
    EXPECT_EQ(r.expect_tag, "v1.0.0");
    EXPECT_TRUE(r.warnings.empty());
}

TEST(PackageManagerRevResolution, MainAndShaWarnAboutStrict) {
    const auto main = resolve("main");
    ASSERT_TRUE(main.status == ResolveStatus::Ok);
    EXPECT_EQ(main.rev, kMain);
    EXPECT_TRUE(main.expect_tag.empty());
    ASSERT_EQ(main.warnings.size(), 1u);
    EXPECT_NE(main.warnings[0].find("STRICT"), std::string::npos);

    const auto sha = resolve(kRoot);
    ASSERT_TRUE(sha.status == ResolveStatus::Ok);
    EXPECT_EQ(sha.rev, kRoot);
    EXPECT_NE(sha.warnings[0].find("STRICT"), std::string::npos);
}

TEST(PackageManagerRevResolution, OffMainIsRefusedUnlessAllowed) {
    EXPECT_TRUE(resolve("feat/x").status == ResolveStatus::InputError);
    EXPECT_TRUE(resolve(kFeature).status == ResolveStatus::InputError);
    EXPECT_TRUE(resolve("v9.9.9-feature").status == ResolveStatus::InputError);

    const auto allowed = resolve("feat/x", true);
    ASSERT_TRUE(allowed.status == ResolveStatus::Ok);
    EXPECT_EQ(allowed.rev, kFeature);
    EXPECT_TRUE(allowed.off_main);
    EXPECT_EQ(allowed.warnings.size(), 2u);  // STRICT and off-main
}

TEST(PackageManagerRevResolution, UnknownAndShortRefsAreInputErrors) {
    EXPECT_TRUE(resolve("nosuch").status == ResolveStatus::InputError);
    EXPECT_TRUE(resolve("3000000").status == ResolveStatus::InputError);  // short SHA
    EXPECT_TRUE(resolve("5000000000000000000000000000000000000000").status == ResolveStatus::InputError);
    EXPECT_TRUE(resolve("").status == ResolveStatus::InputError);
}

TEST(PackageManagerRevResolution, UnreachableSourceIsAnIoError) {
    FakeRevs revs;
    revs.online = false;
    RevRequest request;
    request.to = "v1.0.0";
    EXPECT_TRUE(resolve_rev(request, revs).status == ResolveStatus::IoError);
}
