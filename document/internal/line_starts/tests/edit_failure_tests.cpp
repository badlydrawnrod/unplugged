#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "document/internal/line_starts/line_starts.h"
#include "test_support/allocation_failure.h"

using unplugged::testing::ExerciseAllocationFailures;
using unplugged::testing::FailAllocationAfter;

TEST(EditFailureTest, LineIndexInsertionPreservesStartsOnFailure) {
  const std::string inserted(32, '\n');
  ExerciseAllocationFailures(
      [] { return LineStarts{std::vector<ByteIndex>{0, 2, 4}}; },
      [&](LineStarts& lines) { lines.UpdateOnInsert(1, AsByteSpan(inserted)); },
      [](const LineStarts& lines) {
        ASSERT_EQ(lines.NumLines(), 3u);
        EXPECT_EQ(lines.StartOfLine(0), 0u);
        EXPECT_EQ(lines.StartOfLine(1), 2u);
        EXPECT_EQ(lines.StartOfLine(2), 4u);
        EXPECT_FALSE(lines.CheckInvariants().has_value());
      },
      [](const LineStarts& lines) {
        ASSERT_EQ(lines.NumLines(), 35u);
        EXPECT_EQ(lines.StartOfLine(0), 0u);
        for (LineStarts::LineNumber line = 1; line <= 32; ++line) {
          EXPECT_EQ(lines.StartOfLine(line), line + 1);
        }
        EXPECT_EQ(lines.StartOfLine(33), 34u);
        EXPECT_EQ(lines.StartOfLine(34), 36u);
        EXPECT_FALSE(lines.CheckInvariants().has_value());
      });
}
