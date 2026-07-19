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

// normalize()/extract_name_hint() only case-fold ASCII (see the comment in
// tag_scorer.cpp); these guard the actual safety requirement — non-ASCII
// UTF-8 bytes must never crash or get corrupted into a false/garbled match,
// even though there is no full Unicode case folding.
TEST(TagScorer, NonAsciiTagsMatchByteForByte) {
    // Identical multibyte (Japanese) tags match.
    EXPECT_EQ(score({"\xe6\xa8\xb9"}, {"\xe6\xa8\xb9"}, "", ""),
              kTagMatchScore);  // "樹" (tree)
    // Different multibyte tags don't accidentally match.
    EXPECT_EQ(score({"\xe6\xa8\xb9"}, {"\xe5\xb2\xa9"}, "", ""), 0);
}

TEST(TagScorer, MixedAsciiAndNonAsciiTagStillFoldsTheAsciiPart) {
    // "Tree_\xe6\xa8\xb9" ("Tree_樹") vs "tree_\xe6\xa8\xb9" — ASCII half
    // should still fold case-insensitively; the non-ASCII half compares
    // unchanged (identical bytes here, so it still matches).
    EXPECT_EQ(score({"Tree_\xe6\xa8\xb9"}, {"tree_\xe6\xa8\xb9"}, "", ""),
              kTagMatchScore);
}

TEST(TagScorerHint, NonAsciiNameHintDoesNotCrashOrCorrupt) {
    // Trim/strip logic must leave a pure-Japanese name intact (nothing to
    // trim, no ASCII "(N)" suffix to strip) instead of misclassifying any
    // of its UTF-8 continuation bytes as whitespace/digits.
    EXPECT_EQ(extract_name_hint("\xe6\xa8\xb9\xe3\x81\xae\xe5\xad\x90"),
              "\xe6\xa8\xb9\xe3\x81\xae\xe5\xad\x90");  // "樹の子"
}
