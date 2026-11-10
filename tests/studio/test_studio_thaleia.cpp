#include "fake_ws.h"
#include "plugins/common/file_io.h"
#include "plugins/musa_thaleia/document_browser.h"
#include "plugins/musa_thaleia/markdown.h"
#include "plugins/musa_thaleia/thaleia_plugin.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>

using namespace ergo::studio;
using namespace ergo::studio::plugins;
using namespace ergo::studio::plugins::musa_thaleia;
using namespace studio_test;
namespace fs = std::filesystem;

TEST(StudioMarkdown, Inlines) {
    const auto v = parse_inlines("a **b *c*** `x` [l](http://u \"t\") ![i](p.png) <https://h> \\*n\\* ~~s~~");
    std::string kinds;
    for (const auto& in : v) {
        kinds += in.kind == InlineKind::Code ? 'C' : in.kind == InlineKind::Link ? 'L' : in.kind == InlineKind::Image ? 'I' : 'T';
    }
    ASSERT_EQ(kinds, "TTTTCTLTITLTT");   // fatal: the indices below assume this shape
    EXPECT_EQ(v[1].text, "b ");
    EXPECT_TRUE(v[1].strong);
    EXPECT_TRUE(v[2].strong && v[2].em);   // nested
    EXPECT_EQ(v[4].text, "x");
    EXPECT_EQ(v[6].url, "http://u");
    EXPECT_EQ(v[8].text, "i");
    EXPECT_EQ(v[10].url, "https://h");
    EXPECT_EQ(v[11].text, " *n* ");        // escapes are literal
    EXPECT_TRUE(v[12].strike);
    // Intraword underscores stay literal; an unclosed delimiter is text.
    EXPECT_EQ(plain_text(parse_inlines("snake_case_name and *open")), "snake_case_name and *open");
    const auto br = parse_inlines("one  \ntwo\nthree");
    ASSERT_EQ(br.size(), 3u);
    EXPECT_EQ(br[1].kind, InlineKind::Break);
    EXPECT_EQ(br[2].text, "two three");
}

TEST(StudioMarkdown, Blocks) {
    const std::string md =
        "# Title #\n"
        "Intro line\nsecond line\n\n"
        "Setext\n---\n\n"
        "```cpp extra\nint x;\n\n  y();\n```\n"
        "> quoted\n> # inner\n\n"
        "- [x] done\n  - child\n1. one\n2) two\n"
        "***\n"
        "| a | b |\n|:--|--:|\n| 1 | 2 \\| 3 |\n| only |\n";
    const auto b = parse_markdown(md);
    ASSERT_EQ(b.size(), 11u);
    ASSERT_EQ(b[10].kind, BlockKind::Table);
    ASSERT_EQ(b[10].rows.size(), 3u);
    EXPECT_EQ(b[0].kind, BlockKind::Heading);
    EXPECT_EQ(plain_text(b[0].inlines), "Title");
    EXPECT_EQ(plain_text(b[1].inlines), "Intro line second line");
    EXPECT_EQ(b[2].level, 2);                     // setext
    EXPECT_EQ(b[3].kind, BlockKind::Code);
    EXPECT_EQ(b[3].lang, "cpp");
    EXPECT_EQ(b[3].code, "int x;\n\n  y();");
    ASSERT_EQ(b[4].kind, BlockKind::Quote);
    ASSERT_EQ(b[4].children.size(), 2u);
    EXPECT_EQ(b[4].children[1].kind, BlockKind::Heading);
    EXPECT_TRUE(b[5].checked.value_or(false));
    EXPECT_EQ(b[6].level, 1);                     // nested by indent
    EXPECT_TRUE(b[7].ordered);
    EXPECT_EQ(b[8].number, 2);
    EXPECT_EQ(b[9].kind, BlockKind::Rule);
    ASSERT_EQ(b[10].kind, BlockKind::Table);
    EXPECT_EQ(b[10].aligns[0], Align::Left);
    EXPECT_EQ(b[10].aligns[1], Align::Right);
    ASSERT_EQ(b[10].rows.size(), 3u);
    EXPECT_EQ(plain_text(b[10].rows[1][1]), "2 | 3");
    EXPECT_EQ(b[10].rows[2].size(), 2u);          // padded

    const auto toc = extract_toc(b);
    ASSERT_EQ(toc.size(), 3u);
    EXPECT_EQ(toc[2].title, "inner");
    EXPECT_EQ(first_h1(b).value_or(""), "Title");
}

TEST(StudioMarkdown, Slugify) {
    EXPECT_EQ(slugify("Hello, World!"), "hello-world");
    EXPECT_EQ(slugify("  a--b__c  "), "a-b__c");
    EXPECT_EQ(slugify("<code>x</code> y"), "x-y");
    EXPECT_EQ(slugify("設計 方針"), "設計-方針");
}

namespace {

class TempDocs {
public:
    TempDocs() {
        static std::atomic<int> n{0};
        root_ = fs::temp_directory_path() / ("ergo_studio_thaleia_" + std::to_string(++n));
        fs::remove_all(root_);
        put("guide/intro.md", "# Intro\nWelcome to the player guide.\n");
        put("guide/Zeta.md", "no title here, mentions Player\n");
        put("api/player_controller.md", "# PlayerController\n## Move\n");
        put("readme.md", "top level\n");
        put("empty/notes.txt", "not markdown");
        put(".git/x.md", "# hidden");
    }
    ~TempDocs() {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }
    const fs::path& root() const { return root_; }

private:
    void put(const std::string& rel, const std::string& text) {
        std::string err;
        write_text_file(root_ / fs::u8path(rel), text, err);
    }
    fs::path root_;
};

} // namespace

TEST(StudioThaleia, TreeReadCategoriesSearchScript) {
    TempDocs docs;
    DocumentBrowser b(docs.root());
    const auto tree = b.file_tree();
    ASSERT_EQ(tree.children.size(), 3u);           // api/, guide/, readme.md (empty/ and .git/ left out)
    EXPECT_TRUE(tree.children[0].is_directory);
    EXPECT_EQ(tree.children[0].name, "api");
    EXPECT_EQ(tree.children[2].name, "readme.md");
    EXPECT_EQ(tree.children[1].children[1].name, "Zeta.md");   // case-insensitive order

    const auto d = b.read("guide/intro.md");
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(d->title, "Intro");
    EXPECT_EQ(d->category.value_or(""), "guide");
    const auto zeta = b.read("guide/Zeta.md");   // EXPECT_EQ binds references: keep it alive
    ASSERT_TRUE(zeta.has_value());
    EXPECT_EQ(zeta->title, "Zeta");
    const auto top = b.read("readme.md");
    ASSERT_TRUE(top.has_value());
    EXPECT_FALSE(top->category.has_value());
    EXPECT_FALSE(b.read("../outside.md").has_value());

    const auto cats = b.categories();
    ASSERT_EQ(cats.size(), 3u);                    // api, empty, guide (dot dirs skipped)
    EXPECT_EQ(cats[0], "api");

    EXPECT_EQ(b.search("PLAYER").size(), 3u);
    EXPECT_EQ(b.search("hidden").size(), 0u);
    EXPECT_NE(b.doc_for_script("player_controller").value_or("").find("player_controller.md"), std::string::npos);
    EXPECT_FALSE(b.doc_for_script("x").has_value());

    const auto pc = b.read("api/player_controller.md");
    ASSERT_TRUE(pc.has_value());
    const auto r = render(pc->content, "p.md", std::nullopt);
    EXPECT_EQ(r.title, "PlayerController");
    EXPECT_EQ(r.toc.size(), 2u);
    const auto untitled = render("no heading", "dir/name.md", std::nullopt);
    EXPECT_EQ(untitled.title, "name");
}

TEST(StudioThaleia, PluginApi) {
    TempDocs docs;
    ThaleiaPlugin p(docs.root());
    net::HttpRequest r = req("GET");
    r.query = {{"path", "guide/intro.md"}};
    JsonValue body;
    ASSERT_TRUE(ergo::common::jsonm::parse(p.handle(r, "/api/render").body, body));
    EXPECT_EQ(field(body, "title")->as_string(), "Intro");
    EXPECT_EQ(field(body, "blocks")->a->at(0).find("kind")->as_string(), "heading");

    r.query = {{"path", "../x.md"}};
    EXPECT_EQ(p.handle(r, "/api/document").status, 404);
    EXPECT_EQ(p.handle(req("POST", "# Hi"), "/api/render").status, 200);
    r.query = {{"name", "intro"}};
    EXPECT_NE(p.handle(r, "/api/script").body.find("intro.md"), std::string::npos);

    ThaleiaPlugin unset{fs::path()};
    EXPECT_EQ(unset.handle(req("GET"), "/api/tree").status, 503);
    EXPECT_FALSE(field(unset.snapshot(), "ok")->as_bool());
    EXPECT_TRUE(default_thaleia_root("").empty());
}
