#include "gtest/gtest.h"
#include "ergo/vector/vector.h"

using namespace ergo::vector;

TEST(VectorSvgParser, ParsesBasicShapesAndPath) {
  const char* svg =
      "<svg><polygon id=\"p\" points=\"0,0 10,0 10,10\" fill-rule=\"evenodd\"/>"
      "<rect id=\"r\" x=\"1\" y=\"2\" width=\"3\" height=\"4\"/></svg>";
  auto paths = parse_svg_data(svg);
  ASSERT_TRUE(paths.size() >= 2u);
  EXPECT_EQ(paths[0].fill_rule, FillRule::EvenOdd);
}

// --- SvgLimits boundaries and hostile input ---------------------------------

namespace {
std::string polyline_with(std::size_t points) {
  std::string s = "<svg><polyline points=\"";
  for (std::size_t i = 0; i < points; ++i) s += "1,2 ";
  return s + "\"/></svg>";
}
} // namespace

TEST(VectorSvgLimits, PointsExactlyAtLimitAndOneOver) {
  SvgLimits limits;
  limits.max_points = 10;
  std::vector<VectorPath> out;
  EXPECT_TRUE(parse_svg_data(polyline_with(10), limits, out));
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0].points.size(), 10u);
  EXPECT_FALSE(parse_svg_data(polyline_with(11), limits, out));
  EXPECT_TRUE(out.empty());
}

TEST(VectorSvgLimits, ShapesExactlyAtLimitAndOneOver) {
  SvgLimits limits;
  limits.max_shapes = 2;
  std::vector<VectorPath> out;
  const std::string two = "<svg><polygon points=\"0,0 1,0 1,1\"/><polygon points=\"0,0 1,0 1,1\"/></svg>";
  EXPECT_TRUE(parse_svg_data(two, limits, out));
  EXPECT_EQ(out.size(), 2u);
  EXPECT_FALSE(parse_svg_data(two.substr(0, two.size() - 6) + "<polygon points=\"0,0 1,1\"/></svg>", limits, out));
}

TEST(VectorSvgLimits, SourceBytesExactlyAtLimitAndOneOver) {
  const std::string svg = "<svg><polygon points=\"0,0 10,0 10,10\"/></svg>";
  SvgLimits limits;
  limits.max_source_bytes = svg.size();
  std::vector<VectorPath> out;
  EXPECT_TRUE(parse_svg_data(svg, limits, out));
  limits.max_source_bytes = svg.size() - 1;
  EXPECT_FALSE(parse_svg_data(svg, limits, out));
}

TEST(VectorSvgLimits, LongTagsDoNotExhaustTheStack) {
  // A 4 MiB attribute: std::regex recursed per character here and crashed.
  std::string svg = "<svg><path id=\"" + std::string(4u << 20, 'a') + "\" d=\"M 0 0 L 1 0 L 1 1 Z\"/></svg>";
  auto paths = parse_svg_data(svg);
  ASSERT_EQ(paths.size(), 1u);
  // M, L, L (a trailing command letter with no operands, the final Z, is
  // dropped by the tokenizer as before).
  EXPECT_EQ(paths[0].points.size(), 3u);
}

TEST(VectorSvgLimits, MalformedNumbersAndTagsAreTolerated) {
  // Non-numbers fall back to the default, out-of-range numbers are skipped:
  // std::stof threw on both before.
  auto paths = parse_svg_data("<svg><rect x=\"abc\" y=\"1e999\" width=\"2\" height=\"2\"/>"
                              "<circle cx=\"x\" r=\"1\"/><polygon points=\"99999999999999999999999999999999999999999,1 2,2 3,3\"/>"
                              "<path d=\"M\"/><><path d=\"M 0 0 L 1 1\" <unclosed");
  EXPECT_EQ(paths.size(), 3u);  // rect, circle, polygon (the paths have no points / no closing '>')
  EXPECT_TRUE(parse_svg_data("").empty());
  EXPECT_TRUE(parse_svg_data("<").empty());
  EXPECT_TRUE(parse_svg_data("<<<<>>>>").empty());
}

TEST(VectorSvgLimits, NumberTokensMatchTheFormerGrammar) {
  // [-+]?[0-9]*\.?[0-9]+ : "1." is "1", ".5" and "-.5" are numbers, "+-3" is "-3", "1e5" is 1 and 5.
  auto paths = parse_svg_data("<svg><polyline points=\"1. .5 -.5 +-3 1e5\"/></svg>");
  ASSERT_EQ(paths.size(), 1u);
  ASSERT_EQ(paths[0].points.size(), 3u);
  EXPECT_FLOAT_EQ(paths[0].points[0].p.x, 1.0f);
  EXPECT_FLOAT_EQ(paths[0].points[0].p.y, 0.5f);
  EXPECT_FLOAT_EQ(paths[0].points[1].p.x, -0.5f);
  EXPECT_FLOAT_EQ(paths[0].points[1].p.y, -3.0f);
  EXPECT_FLOAT_EQ(paths[0].points[2].p.x, 1.0f);
  EXPECT_FLOAT_EQ(paths[0].points[2].p.y, 5.0f);
}
