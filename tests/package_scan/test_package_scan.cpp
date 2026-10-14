// ergo_package_scan: 見本ディレクトリ (tests/package_scan/fixtures) からの辺抽出、 根拠の付与、
// 根拠の無い辺を作らないこと、 範囲外を読まないこと、 未観測の残し方、 gitlink の読み取り。

#include "gtest/gtest.h"

#include "ergo/package_scan/git_index_reader.h"
#include "ergo/package_scan/gitmodules_reader.h"
#include "ergo/package_scan/json_lines.h"
#include "ergo/package_scan/manifest_reader.h"
#include "ergo/package_scan/package_json_reader.h"
#include "ergo/package_scan/scan.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace ergo::package_scan;

namespace {

namespace fs = std::filesystem;

const std::string kFixtures = ERGO_PACKAGE_SCAN_FIXTURES;

ScanOptions fixture_options() {
    ScanOptions o;
    o.manifest_path = kFixtures + "/ergo/ergo-packages.json";
    o.root          = kFixtures + "/root";
    return o;
}

UsageModel scan_fixture() {
    UsageModel m;
    scan_usage(fixture_options(), m);
    return m;
}

bool has_kind(const UsageEdge& e, UsageEdgeKind kind) {
    for (const auto k : e.kinds) {
        if (k == kind) return true;
    }
    return false;
}

bool contains(const std::vector<std::string>& list, const std::string& needle) {
    for (const auto& s : list) {
        if (s.find(needle) != std::string::npos) return true;
    }
    return false;
}

void put32(std::string& b, std::uint32_t v) {
    for (int shift = 24; shift >= 0; shift -= 8) b += static_cast<char>((v >> shift) & 0xff);
}

/// git の index (版 2) を組み立てる。 sha は 1 バイトの値を 20 回並べたもの。
std::string make_index(const std::vector<std::pair<std::string, std::uint32_t>>& entries,
                       char sha_byte) {
    std::string b = "DIRC";
    put32(b, 2);
    put32(b, static_cast<std::uint32_t>(entries.size()));
    for (const auto& [name, mode] : entries) {
        const std::size_t start = b.size();
        for (int i = 0; i < 6; ++i) put32(b, 0);   // ctime, mtime, dev, ino
        put32(b, mode);
        for (int i = 0; i < 3; ++i) put32(b, 0);   // uid, gid, size
        b += std::string(20, sha_byte);
        b += static_cast<char>((name.size() >> 8) & 0x0f);
        b += static_cast<char>(name.size() & 0xff);
        b += name;
        const std::size_t used = b.size() - start;
        b += std::string(((used + 8) / 8) * 8 - used, '\0');
    }
    return b;
}

std::string utf8(const fs::path& path) {
    const auto s = path.u8string();
    return std::string(s.begin(), s.end());
}

void write_file(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << text;
}

} // namespace

// ---------------------------------------------------------------- 読み取り部品

TEST(JsonLines, IndexesNestedKeysAndArrayItems) {
    const std::string text = "{\n  \"a\": {\n    \"b\": \"x\",\n    \"c\": [\n      1,\n      {\"d\": true}\n    ]\n  }\n}\n";
    const auto idx = JsonLineIndex::build(text);
    EXPECT_EQ(idx.line_of({"a"}), 2);
    EXPECT_EQ(idx.line_of({"a", "b"}), 3);
    EXPECT_EQ(idx.line_of({"a", "c", "0"}), 5);
    EXPECT_EQ(idx.line_of({"a", "c", "1", "d"}), 6);
    EXPECT_EQ(idx.line_of({"missing"}), 0);
}

TEST(Gitmodules, ReadsPathAndUrlWithLines) {
    const auto subs = parse_gitmodules("# c\n[submodule \"lib/x\"]\n\tpath = lib/x\n\turl = https://h/o/X.git\n"
                                       "[core]\n\tpath = ignored\n[submodule \"nopath\"]\n\turl = u\n");
    ASSERT_EQ(subs.size(), 1u);
    EXPECT_EQ(subs[0].name, "lib/x");
    EXPECT_EQ(subs[0].path, "lib/x");
    EXPECT_EQ(subs[0].path_line, 3);
    EXPECT_EQ(subs[0].url_line, 4);
    EXPECT_EQ(repo_name_from_url(subs[0].url), "X");
    EXPECT_EQ(repo_name_from_url("git@h:o/Y/"), "Y");
}

TEST(PackageJson, KeepsOnlyUsageDependencies) {
    PackageJson p;
    ASSERT_TRUE(parse_package_json("{\n\"name\": \"n\",\n\"dependencies\": {\n\"a\": \"file:../a\",\n"
                                   "\"b\": \"^1\",\n\"@ludiars/c\": \"^2\"\n},\n\"devDependencies\": "
                                   "{\n\"d\": \"workspace:*\"\n}\n}\n",
                                   p));
    EXPECT_EQ(p.name, "n");
    ASSERT_EQ(p.deps.size(), 3u);   // b (公開パッケージ) は根拠にならない
    EXPECT_EQ(p.deps[0].name, "@ludiars/c");
    EXPECT_TRUE(p.deps[0].kind == DepSpecKind::Scoped);
    EXPECT_EQ(p.deps[0].line, 6);
    EXPECT_EQ(p.deps[1].name, "a");
    EXPECT_TRUE(p.deps[1].kind == DepSpecKind::File);
    EXPECT_EQ(p.deps[1].line, 4);
    EXPECT_EQ(p.deps[2].section, "devDependencies");
    EXPECT_TRUE(p.deps[2].kind == DepSpecKind::Workspace);
    EXPECT_FALSE(parse_package_json("{ broken", p));
}

TEST(GitIndex, ReadsOnlyGitlinks) {
    const std::string bytes = make_index({{"README.md", 0100644}, {"lib/lapilli", 0160000}}, '\x12');
    std::map<std::string, std::string> links;
    ASSERT_TRUE(parse_index_gitlinks(bytes, links));
    ASSERT_EQ(links.size(), 1u);
    std::string expected;
    for (int i = 0; i < 20; ++i) expected += "12";
    EXPECT_EQ(links["lib/lapilli"], expected);
    // 版 4 (接頭辞圧縮) は読まない。
    std::string v4 = bytes;
    v4[7] = 4;
    EXPECT_FALSE(parse_index_gitlinks(v4, links));
    EXPECT_FALSE(parse_index_gitlinks("nope", links));
}

TEST(Manifest, ReadsObservePortsAndToolsWithLines) {
    std::string text;
    {
        std::ifstream in(fs::u8path(kFixtures + "/ergo/ergo-packages.json"), std::ios::binary);
        text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    ScanManifest m;
    ASSERT_TRUE(parse_scan_manifest(text, m));
    ASSERT_EQ(m.ports.size(), 1u);
    EXPECT_EQ(m.ports[0].module, "ergo_resource");
    EXPECT_EQ(m.ports[0].package, "clio");
    EXPECT_EQ(m.ports[0].line, 10);
    EXPECT_EQ(m.ports[0].key, "modules.ergo_resource.ported_from[0]");
    ASSERT_EQ(m.tools.size(), 1u);
    EXPECT_EQ(m.tools[0].path, "lib/lapilli");
    EXPECT_EQ(m.observe.size(), 6u);
    EXPECT_FALSE(parse_scan_manifest("{\"schema\": 2}", m));
}

// ---------------------------------------------------------------- 辺抽出と根拠

TEST(Scan, ExtractsSubmoduleAndFileEdgesWithEvidence) {
    UsageModel m;
    std::string error;
    ASSERT_TRUE(scan_usage(fixture_options(), m, &error));

    const auto* e = m.find_edge("thaleia", "lapilli");
    ASSERT_TRUE(e != nullptr);
    EXPECT_TRUE(has_kind(*e, UsageEdgeKind::Submodule));
    EXPECT_TRUE(has_kind(*e, UsageEdgeKind::FileDep));
    EXPECT_FALSE(e->dashed());
    ASSERT_EQ(e->evidence.size(), 2u);
    EXPECT_EQ(e->evidence[0].file, "Thaleia/.gitmodules");
    EXPECT_EQ(e->evidence[0].line, 4);
    EXPECT_EQ(e->evidence[0].key, "submodule.lib/lapilli.url");
    EXPECT_EQ(e->evidence[0].note, "gitlink 未読");   // 見本に .git は無い
    EXPECT_EQ(e->evidence[1].file, "Thaleia/package.json");
    EXPECT_EQ(e->evidence[1].line, 5);
    EXPECT_EQ(e->evidence[1].key, "dependencies.@ludiars/one-shot");

    const auto* tools = m.find_edge("tools/ergo", "lapilli");
    ASSERT_TRUE(tools != nullptr);
    EXPECT_EQ(tools->evidence[0].file, "ergo:tools/ergo/package.json");
    EXPECT_EQ(tools->evidence[0].line, 5);

    const auto* sub = m.find_edge("ergo", "lapilli");
    ASSERT_TRUE(sub != nullptr);
    EXPECT_EQ(sub->evidence[0].file, "ergo:.gitmodules");
    EXPECT_EQ(sub->evidence[0].line, 3);
}

TEST(Scan, ResolvesScopedDependencyThroughObservedPackageName) {
    const auto m = scan_fixture();
    const auto* e = m.find_edge("thaleia", "terpsichore");
    ASSERT_TRUE(e != nullptr);
    EXPECT_TRUE(has_kind(*e, UsageEdgeKind::ScopedDep));
    EXPECT_EQ(e->evidence[0].line, 6);
    const auto* ws = m.find_edge("terpsichore", "npm:shared-kit");
    ASSERT_TRUE(ws != nullptr);
    EXPECT_TRUE(has_kind(*ws, UsageEdgeKind::WorkspaceDep));
    ASSERT_TRUE(m.find_node("npm:shared-kit") != nullptr);
    EXPECT_TRUE(m.find_node("npm:shared-kit")->kind == UsageKind::SharedTs);
}

TEST(Scan, ManifestRelationsBecomeDashedEdges) {
    const auto m = scan_fixture();
    const auto* port = m.find_edge("ergo_resource", "clio");
    ASSERT_TRUE(port != nullptr);
    EXPECT_TRUE(port->dashed());
    EXPECT_EQ(port->evidence[0].file, "ergo:ergo-packages.json");
    EXPECT_EQ(port->evidence[0].line, 10);
    const auto* vocab = m.find_edge("ergo_resource", "curare");
    ASSERT_TRUE(vocab != nullptr);
    EXPECT_TRUE(vocab->dashed());
    EXPECT_TRUE(has_kind(*vocab, UsageEdgeKind::SharesVocabulary));
    EXPECT_EQ(vocab->evidence[0].line, 22);
    EXPECT_EQ(vocab->evidence.size(), 1u);   // 未知の kind (rumoured) は辺にしない
    EXPECT_TRUE(contains(m.notes, "rumoured"));
    ASSERT_TRUE(m.find_node("ergo_resource") != nullptr);
    EXPECT_TRUE(m.find_node("ergo_resource")->kind == UsageKind::Ergo);
}

TEST(Scan, EveryEdgeHasEvidence) {
    const auto m = scan_fixture();
    EXPECT_GT(m.edges.size(), 0u);
    for (const auto& e : m.edges) {
        EXPECT_FALSE(e.evidence.empty());
        for (const auto& ev : e.evidence) {
            EXPECT_FALSE(ev.file.empty());
            EXPECT_FALSE(ev.key.empty());
            EXPECT_GT(ev.line, 0);
        }
    }
}

TEST(Scan, NoEdgesWithoutEvidence) {
    const auto m = scan_fixture();
    // 公開パッケージ (left-pad / express) と、 宣言だけで根拠の無い needs (ergo_render) は辺にならない。
    EXPECT_TRUE(m.find_node("npm:left-pad") == nullptr);
    EXPECT_TRUE(m.find_node("npm:express") == nullptr);
    EXPECT_TRUE(m.find_node("ergo_render") == nullptr);
    // Ergo 本体とモジュール・ツールの間に推測の辺を作らない。
    EXPECT_TRUE(m.find_edge("ergo", "ergo_resource") == nullptr);
    EXPECT_TRUE(m.find_edge("ergo", "tools/ergo") == nullptr);
    // チェックアウトはあっても package.json も .gitmodules も無い Clio から辺は出ない。
    for (const auto& e : m.edges) EXPECT_TRUE(e.from != "clio");
}

// ---------------------------------------------------------------- 範囲

TEST(Scan, DoesNotReadOutsideDeclaredCheckouts) {
    const auto m = scan_fixture();
    // root/Outside は observe に無い (escape の path_hint "../Outside" は範囲外)。
    EXPECT_FALSE(contains(m.files_read, "Outside"));
    EXPECT_TRUE(m.find_node("npm:@ludiars/must-not-be-read") == nullptr);
    for (const auto& f : m.files_read) {
        const bool ergo = f.rfind("ergo:", 0) == 0;
        const bool declared = f.rfind("Thaleia/", 0) == 0 || f.rfind("Terpsichore/", 0) == 0 ||
                              f.rfind("Clio/", 0) == 0 || f.rfind("Curare/", 0) == 0;
        EXPECT_TRUE(ergo || declared);
    }
    // Outside を指す file: 依存はパスの計算だけで帰属し、 中身は読まない。
    const auto* e = m.find_edge("terpsichore", "repo:Outside");
    ASSERT_TRUE(e != nullptr);
    EXPECT_EQ(e->evidence[0].file, "Terpsichore/package.json");
}

TEST(Scan, RecordsFilesReadInNameOrder) {
    const auto m = scan_fixture();
    const std::vector<std::string> expected = {
        "Terpsichore/package.json", "Thaleia/.gitmodules", "Thaleia/package.json",
        "ergo:.gitmodules",         "ergo:ergo-packages.json", "ergo:tools/ergo/package.json"};
    EXPECT_EQ(m.files_read, expected);
}

// ---------------------------------------------------------------- 未観測

TEST(Scan, KeepsMissingAndOutOfRangeEntriesAsUnobserved) {
    const auto m = scan_fixture();
    const auto* musa = m.find_node("ars-musa");
    ASSERT_TRUE(musa != nullptr);
    EXPECT_TRUE(musa->kind == UsageKind::Unobserved);
    EXPECT_EQ(musa->reason, "チェックアウトなし");
    EXPECT_EQ(musa->declared_group, "musa");
    const auto* esc = m.find_node("escape");
    ASSERT_TRUE(esc != nullptr);
    EXPECT_TRUE(esc->kind == UsageKind::Unobserved);
    EXPECT_EQ(esc->reason, "path_hint が範囲外");
    // 読めた観測対象は宣言の group のまま (辺が無くても出す)。
    EXPECT_TRUE(m.find_node("clio")->kind == UsageKind::Musa);
    EXPECT_TRUE(m.find_node("curare")->kind == UsageKind::Related);
}

TEST(Scan, WithoutRootEveryObservedEntryIsUnobserved) {
    auto o = fixture_options();
    o.root.clear();
    UsageModel m;
    ASSERT_TRUE(scan_usage(o, m));
    for (const auto& n : m.nodes) {
        if (n.declared_group.empty()) continue;
        EXPECT_TRUE(n.kind == UsageKind::Unobserved);
        EXPECT_EQ(n.reason, "--root 未指定");
    }
    // マニフェストの根拠を持つ辺 (移植・語彙共有) と Ergo 自身の辺は残る。
    EXPECT_TRUE(m.find_edge("ergo_resource", "clio") != nullptr);
    EXPECT_TRUE(m.find_edge("tools/ergo", "lapilli") != nullptr);
    EXPECT_TRUE(m.find_edge("thaleia", "lapilli") == nullptr);
}

TEST(Scan, MissingManifestIsAnError) {
    ScanOptions o;
    o.manifest_path = kFixtures + "/ergo/no-such.json";
    UsageModel m;
    std::string error;
    EXPECT_FALSE(scan_usage(o, m, &error));
    EXPECT_FALSE(error.empty());
}

// ---------------------------------------------------------------- gitlink

TEST(Scan, ReadsGitlinkShaFromIndexInsideCheckout) {
    const fs::path tmp = fs::temp_directory_path() / "ergo_package_scan_gitlink";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    write_file(tmp / "ergo" / "ergo-packages.json",
               "{\"schema\": 1, \"packages\": {\"lapilli\": {\"kind\": \"tool\", \"path\": \"lib/lapilli\"}},\n"
               " \"observe\": {\"thaleia\": {\"path_hint\": \"Thaleia\", \"group\": \"musa\"}}}\n");
    write_file(tmp / "root" / "Thaleia" / ".gitmodules",
               "[submodule \"lib/lapilli\"]\n\tpath = lib/lapilli\n\turl = https://h/o/Lapilli.git\n");
    write_file(tmp / "root" / "Thaleia" / ".git" / "index",
               make_index({{"lib/lapilli", 0160000}}, '\xab'));

    ScanOptions o;
    o.manifest_path = utf8(tmp / "ergo" / "ergo-packages.json");
    o.root          = utf8(tmp / "root");
    UsageModel m;
    ASSERT_TRUE(scan_usage(o, m));
    const auto* e = m.find_edge("thaleia", "lapilli");
    ASSERT_TRUE(e != nullptr);
    EXPECT_EQ(e->evidence[0].note, "gitlink abababa");
    EXPECT_TRUE(contains(m.files_read, "Thaleia/.git/index"));
    fs::remove_all(tmp, ec);
}

TEST(Scan, IsDeterministic) {
    const auto a = scan_fixture();
    const auto b = scan_fixture();
    ASSERT_EQ(a.nodes.size(), b.nodes.size());
    ASSERT_EQ(a.edges.size(), b.edges.size());
    for (std::size_t i = 0; i < a.edges.size(); ++i) {
        EXPECT_EQ(a.edges[i].from, b.edges[i].from);
        EXPECT_EQ(a.edges[i].to, b.edges[i].to);
        EXPECT_EQ(describe(a.edges[i].evidence[0]), describe(b.edges[i].evidence[0]));
    }
}
