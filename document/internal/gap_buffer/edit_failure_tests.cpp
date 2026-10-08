#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "document/internal/gap_buffer/gap_buffer.h"
#include "test_support/allocation_failure.h"

using unplugged::testing::ExerciseAllocationFailures;
using unplugged::testing::FailAllocationAfter;

namespace {
std::string Snapshot(const GapBuffer& buffer) {
  std::vector<Byte> bytes;
  EXPECT_EQ(buffer.AppendRange(0, buffer.Len(), bytes), buffer.Len());
  return std::string(bytes.begin(), bytes.end());
}
}  // namespace

TEST(EditFailureTest, GapReplacementPreservesBytesOnFailure) {
  const std::string inserted(128, 'x');
  ExerciseAllocationFailures(
      [] { return GapBuffer{std::vector<Byte>{'a', 'b', 'c', 'd'}}; },
      [&](GapBuffer& buffer) { buffer.Replace(1, 2, AsByteSpan(inserted)); },
      [](const GapBuffer& buffer) {
        EXPECT_EQ(Snapshot(buffer), "abcd");
        EXPECT_FALSE(buffer.CheckInvariants().has_value());
      },
      [&](const GapBuffer& buffer) {
        EXPECT_EQ(Snapshot(buffer), "a" + inserted + "d");
        EXPECT_FALSE(buffer.CheckInvariants().has_value());
      });
}
