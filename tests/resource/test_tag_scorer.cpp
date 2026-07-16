#include "ergo/resource/tag_scorer.h"

#include "gtest/gtest.h"

using ergo::resource::extract_name_hint;
using ergo::resource::kNameExactScore;
using ergo::resource::kNameTokenScore;
using ergo::resource::kTagMatchScore;
using ergo::resource::score;

TEST(TagScorer, CountsEachMatchingTag) {
    EXPECT_EQ(score({"tree", "nature"}, {"tree"}, "", ""), kTagMatchScore);
    EXPECT_EQ(score({"tree", "nature"}, {"tree", "nature"}, "", ""),
              2 * kTagMatchScore);
    EXPECT_EQ(score({"rock"}, {"tree"}, "", ""), 0);
}

TEST(TagScorer, TagComparisonIsCaseInsensitive) {
    EXPECT_EQ(score({"Tree"}, {"tRee"}, "", ""), kTagMatchScore);
    EXPECT_EQ(score({"  tree  "}, {"tree"}, "", ""), kTagMatchScore);
}

TEST(TagScorer, BlankTagsAreIgnored) {
    EXPECT_EQ(score({"", "  "}, {"", "tree"}, "", ""), 0);
}

TEST(TagScorer, ExactNameMatchAddsBonus) {
    EXPECT_EQ(score({}, {}, "tree01", "Tree01"), kNameExactScore);
}

TEST(TagScorer, NameTokensAddPerTokenBonus) {
    // hint "big tree" → tokens "big" + "tree", both contained
    EXPECT_EQ(score({}, {}, "big_tree_01", "big tree"), 2 * kNameTokenScore);
    // single-char tokens are ignored
    EXPECT_EQ(score({}, {}, "abc", "a b"), 0);
}

TEST(TagScorer, TagsDominateNames) {
    const int tag_only  = score({"tree"}, {"tree"}, "unrelated", "");
    const int name_only = score({}, {"tree"}, "tree", "tree");
    EXPECT_GT(tag_only, name_only);
}

TEST(TagScorerHint, StripsDuplicateCounter) {
    EXPECT_EQ(extract_name_hint("tree_big (2)"), "tree_big");
    EXPECT_EQ(extract_name_hint("  tree  "), "tree");
}

TEST(TagScorerHint, PlaceholderDefaultNameYieldsEmptyHint) {
    EXPECT_EQ(extract_name_hint("Cube"), "");
    EXPECT_EQ(extract_name_hint("cube (3)"), "");
    EXPECT_EQ(extract_name_hint(""), "");
}
