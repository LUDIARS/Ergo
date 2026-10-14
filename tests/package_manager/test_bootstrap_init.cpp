// ブートストラップの生成と古さ、 `init` の計画 (spec/feature/package-manager.md §7 W1:
// init を 2 回走らせて 2 回目が無変更・.gitignore に .ergo/ が 1 行、 doctor が古さを真偽両方で検出)。

#include "fixture_util.h"

#include "ergo/package_manager/bootstrap.h"
#include "ergo/package_manager/host_declaration.h"
#include "ergo/package_manager/init_plan.h"
#include "ergo/package_manager/leak_check.h"

#include "gtest/gtest.h"

#include <optional>

using namespace ergo::package_manager;
using ergo::package_manager::test::bootstrap_template;

namespace {

const std::string kSha = "625d83d61c90c30ceed3fab2bd2e11a7edf8d39f";

/// 計画を「ファイル」に当てる (テストの中のホスト)。
void apply(const InitPlan& plan, ExistingFiles& files) {
    for (const auto& w : plan.writes) {
        if (w.path == kHostDeclarationPath) files.declaration = w.content;
        else if (w.path == kBootstrapPath) files.bootstrap = w.content;
        else if (w.path == kGitignorePath) files.gitignore = w.content;
        else if (w.path == kUpmHostPath) files.upm_host = w.content;
    }
}

InitOptions options(const std::string& code = "Example") {
    InitOptions o;
    o.code               = code;
    o.ergo_rev           = kSha;
    o.bootstrap_template = bootstrap_template();
    return o;
}

std::size_t count_lines(const std::string& text, const std::string& line) {
    std::size_t n = 0;
    std::size_t begin = 0;
    while (begin < text.size()) {
        std::size_t end = text.find('\n', begin);
        if (end == std::string::npos) end = text.size();
        if (text.substr(begin, end - begin) == line) ++n;
        begin = end + 1;
    }
    return n;
}

} // namespace

TEST(PackageManagerBootstrap, TemplateCarriesTheSourceLine) {
    const auto tmpl = bootstrap_template();
    ASSERT_FALSE(tmpl.empty());
    EXPECT_TRUE(tmpl.find(kBootstrapPlaceholder) != std::string::npos);
    EXPECT_TRUE(find_text_leaks(tmpl).empty());
    const auto text = render_bootstrap(tmpl, kSha);
    EXPECT_EQ(bootstrap_source(text), kSha);
    EXPECT_TRUE(text.find(kBootstrapPlaceholder) == std::string::npos);
}

TEST(PackageManagerBootstrap, FreshnessTrueAndFalse) {
    const auto tmpl = bootstrap_template();
    const auto text = render_bootstrap(tmpl, kSha);
    EXPECT_TRUE(check_bootstrap(nullptr, tmpl).state == BootstrapState::Missing);
    EXPECT_TRUE(check_bootstrap(&text, tmpl).state == BootstrapState::Current);

    // 同じ中身の CRLF 版は古くない (autocrlf)。
    std::string crlf;
    for (const char c : text) crlf += (c == '\n') ? std::string("\r\n") : std::string(1, c);
    EXPECT_TRUE(check_bootstrap(&crlf, tmpl).state == BootstrapState::Current);

    // ひな形が変わった (または手で直した) ものは古い。
    const std::string edited = text + "# local edit\n";
    const auto stale = check_bootstrap(&edited, tmpl);
    EXPECT_TRUE(stale.state == BootstrapState::Stale);
    EXPECT_EQ(stale.source, kSha);
    const auto newer = tmpl + "# newer template line\n";
    EXPECT_TRUE(check_bootstrap(&text, newer).state == BootstrapState::Stale);

    const std::string hand = "add_subdirectory(../Ergo ergo)\n";
    EXPECT_TRUE(check_bootstrap(&hand, tmpl).state == BootstrapState::Foreign);
}

TEST(PackageManagerInit, SecondRunChangesNothing) {
    ExistingFiles files;
    files.gitignore = std::string("build/\n");
    const auto first = plan_init(options(), files);
    ASSERT_TRUE(first.ok);
    EXPECT_EQ(first.writes.size(), static_cast<std::size_t>(3));
    apply(first, files);

    const auto second = plan_init(options(), files);
    ASSERT_TRUE(second.ok);
    EXPECT_TRUE(second.writes.empty());
    EXPECT_EQ(count_lines(*files.gitignore, ".ergo/"), static_cast<std::size_t>(1));
    EXPECT_EQ(*files.gitignore, std::string("build/\n.ergo/\n"));

    ergo::common::jsonm::JsonValue doc;
    ASSERT_TRUE(ergo::common::jsonm::parse(*files.declaration, doc));
    HostDeclaration h;
    std::vector<Finding> read;
    ASSERT_TRUE(read_host_declaration(doc, h, read));
    EXPECT_TRUE(read.empty());
    EXPECT_EQ(h.code, std::string("Example"));
    EXPECT_EQ(h.ergo.rev, kSha);
    EXPECT_TRUE(find_leaks(doc).empty());
}

TEST(PackageManagerInit, StaleBootstrapIsRewrittenForeignIsKept) {
    ExistingFiles files;
    apply(plan_init(options(), files), files);
    files.bootstrap = *files.bootstrap + "# local edit\n";
    auto plan = plan_init(options(), files);
    ASSERT_EQ(plan.writes.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(plan.writes[0].path, std::string(kBootstrapPath));

    files.bootstrap = std::string("add_subdirectory(../Ergo ergo)\n");
    plan = plan_init(options(), files);
    EXPECT_TRUE(plan.writes.empty());
}

TEST(PackageManagerInit, GitignoreVariants) {
    std::string out;
    EXPECT_FALSE(add_gitignore_entry("/.ergo\n", out));
    EXPECT_FALSE(add_gitignore_entry("  .ergo/  \n", out));
    ASSERT_TRUE(add_gitignore_entry("", out));
    EXPECT_EQ(out, std::string(".ergo/\n"));
    ASSERT_TRUE(add_gitignore_entry("build/", out));
    EXPECT_EQ(out, std::string("build/\n.ergo/\n"));
    ASSERT_TRUE(add_gitignore_entry("build/\r\n", out));
    EXPECT_EQ(out, std::string("build/\r\n.ergo/\r\n"));
}

TEST(PackageManagerInit, ExistingErgoManifestIsRefused) {
    ExistingFiles files;
    files.declaration = ergo::package_manager::test::fixture_text("ergo-manifest.json");
    const auto plan = plan_init(options(), files);
    EXPECT_FALSE(plan.ok);
    EXPECT_TRUE(plan.writes.empty());
}

TEST(PackageManagerInit, BadCodeIsRefused) {
    EXPECT_FALSE(plan_init(options("Ex ample"), ExistingFiles{}).ok);
    EXPECT_FALSE(plan_init(options("\"x"), ExistingFiles{}).ok);
    EXPECT_TRUE(is_valid_code("Ln"));
}

TEST(PackageManagerInit, UpmWritesOnlyHostJson) {
    InitOptions o;
    o.code = "Example";
    o.upm  = true;
    ExistingFiles files;
    const auto first = plan_init(o, files);
    ASSERT_EQ(first.writes.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(first.writes[0].path, std::string(kUpmHostPath));
    EXPECT_EQ(first.writes[0].content, std::string("{\n  \"schema\": 1,\n  \"code\": \"Example\"\n}\n"));
    apply(first, files);
    EXPECT_TRUE(plan_init(o, files).writes.empty());
}
