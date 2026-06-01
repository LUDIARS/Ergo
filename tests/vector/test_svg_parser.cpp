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
