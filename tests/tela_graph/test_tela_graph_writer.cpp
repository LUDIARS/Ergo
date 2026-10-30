#include "gtest/gtest.h"

#include "ergo/tela_graph/writer.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace ergo::tela_graph;

namespace {

Graph minimal() {
    Graph g;
    g.project = "P";
    g.title   = "T";
    g.width   = 100;
    g.height  = 100;
    g.groups  = {{"g", "G", true, {10, 20, 30}}};
    g.nodes   = {{"g", "a", "A", 0, 0, 10, 10}, {"g", "b", "B", 50, 0, 10, 10}};
    Edge e;
    e.id     = "a:b";
    e.from   = "a";
    e.to     = "b";
    e.points = {{10, 5}, {50, 5}};
    g.edges  = {e};
    return g;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

} // namespace

TEST(TelaGraphWriter, WritesTheRecordOrderTelaReads) {
    const std::string expected =
        "TELA_GRAPH 1\n"
        "graph \"P\" \"T\" 100 100\n"
        "group \"g\" \"G\" 1 10 20 30\n"
        "node \"g\" \"a\" \"A\" 0 0 10 10\n"
        "node \"g\" \"b\" \"B\" 50 0 10 10\n"
        "edge \"a:b\" \"a\" \"b\" 0\n"
        "point \"a:b\" 10 5\n"
        "point \"a:b\" 50 5\n";
    EXPECT_EQ(to_tela_graph_text(minimal()), expected);
}

TEST(TelaGraphWriter, EscapesQuotesAndBackslashesLikeTela) {
    auto g = minimal();
    g.nodes[0].label = "say \"hi\" \\ there";
    const auto text = to_tela_graph_text(g);
    EXPECT_NE(text.find("\"say \\\"hi\\\" \\\\ there\""), std::string::npos);
}

TEST(TelaGraphWriter, ControlCharactersCannotBreakTheLineFormat) {
    auto g = minimal();
    g.nodes[0].label = "line1\nline2\tend";
    const auto text = to_tela_graph_text(g);
    EXPECT_NE(text.find("\"line1 line2 end\""), std::string::npos);
    // レコード数は改行の数と一致する (ラベルの改行で行が増えない)。
    std::size_t lines = 0;
    for (char c : text) lines += c == '\n';
    EXPECT_EQ(lines, 8u);
}

TEST(TelaGraphWriter, NumbersUseTwoDecimalsWithoutTrailingZeros) {
    auto g = minimal();
    g.edges[0].points = {{10.5f, 5.126f}, {50.0f, -0.001f}};
    const auto text = to_tela_graph_text(g);
    EXPECT_NE(text.find("point \"a:b\" 10.5 5.13\n"), std::string::npos);
    EXPECT_NE(text.find("point \"a:b\" 50 0\n"), std::string::npos);
}

TEST(TelaGraphWriter, RejectsWhatTelaRejects) {
    std::string error;

    auto g = minimal();
    g.nodes[1].id = "a";
    EXPECT_FALSE(validate_tela_graph(g, &error));
    EXPECT_NE(error.find("duplicate node"), std::string::npos);

    g = minimal();
    g.nodes[0].group_id = "missing";
    EXPECT_FALSE(validate_tela_graph(g, &error));

    g = minimal();
    g.edges[0].to = "nowhere";
    EXPECT_FALSE(validate_tela_graph(g, &error));

    g = minimal();
    g.edges[0].points.resize(1);
    EXPECT_FALSE(validate_tela_graph(g, &error));

    g = minimal();
    g.nodes[0].width = 0;
    EXPECT_FALSE(validate_tela_graph(g, &error));

    g = minimal();
    g.nodes[0].x = 200000;
    EXPECT_FALSE(validate_tela_graph(g, &error));

    g = minimal();
    g.nodes[0].id = std::string(257, 'x');
    EXPECT_FALSE(validate_tela_graph(g, &error));

    g = minimal();
    g.groups.clear();
    EXPECT_FALSE(validate_tela_graph(g, &error));

    g = minimal();
    for (int i = 0; i < 8; ++i) g.groups.push_back({"g" + std::to_string(i), "x", true, {}});
    EXPECT_FALSE(validate_tela_graph(g, &error));
    EXPECT_NE(error.find("groups"), std::string::npos);
}

TEST(TelaGraphWriter, RejectsTooManyNodes) {
    auto g = minimal();
    for (int i = 0; i < 127; ++i) {
        g.nodes.push_back({"g", "n" + std::to_string(i), "N", 0, 0, 1, 1});
    }
    std::string error;
    EXPECT_FALSE(validate_tela_graph(g, &error));
    EXPECT_TRUE(to_tela_graph_text(g, &error).empty());
}

TEST(TelaGraphWriter, SaveWritesFileAndFailureLeavesExistingFileUntouched) {
    const auto dir  = std::filesystem::temp_directory_path() / "ergo_tela_graph_writer_test";
    std::filesystem::create_directories(dir);
    const auto path = dir / "graph.tela";

    std::string error;
    ASSERT_TRUE(save_tela_graph(path.string(), minimal(), &error));
    const auto written = read_file(path);
    EXPECT_EQ(written, to_tela_graph_text(minimal()));

    auto broken = minimal();
    broken.nodes.clear();
    EXPECT_FALSE(save_tela_graph(path.string(), broken, &error));
    EXPECT_EQ(read_file(path), written);
    EXPECT_FALSE(std::filesystem::exists(dir / "graph.tela.tmp"));

    std::filesystem::remove_all(dir);
}

TEST(TelaGraphWriter, SaveCreatesMissingParentDirectories) {
    const auto dir  = std::filesystem::temp_directory_path() / "ergo_tela_graph_writer_mkdir_test";
    std::filesystem::remove_all(dir);
    const auto path = dir / "nested" / "deeper" / "graph.tela";

    std::string error;
    ASSERT_TRUE(save_tela_graph(path.string(), minimal(), &error));
    EXPECT_EQ(read_file(path), to_tela_graph_text(minimal()));
    EXPECT_FALSE(std::filesystem::exists(dir / "nested" / "deeper" / "graph.tela.tmp"));

    // A bare relative file name has no parent folder to create and still works.
    const auto cwd = std::filesystem::current_path();
    std::filesystem::current_path(dir);
    const bool bare = save_tela_graph("bare.tela", minimal(), &error);
    std::filesystem::current_path(cwd);
    EXPECT_TRUE(bare);
    EXPECT_EQ(read_file(dir / "bare.tela"), to_tela_graph_text(minimal()));

    std::filesystem::remove_all(dir);
}
