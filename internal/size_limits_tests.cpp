#include <gtest/gtest.h>

#include <limits>

#include "internal/size_limits.h"

namespace unplugged::internal {
namespace {

static_assert(kMaxDocumentBytes < std::numeric_limits<ByteCount>::max());
static_assert(kMaxStorageBytes <= std::numeric_limits<std::ptrdiff_t>::max());
static_assert(CheckedByteCount(kMaxDocumentBytes, kMaxDocumentBytes) ==
              kMaxDocumentBytes);
static_assert(CheckedByteGrowth(kMaxDocumentBytes - 1, 1, kMaxDocumentBytes) ==
              kMaxDocumentBytes);
static_assert(GapGrowth(kMaxStorageBytes - 2, 0, 1) == 2);
static_assert(GapGrowth(kMaxStorageBytes, 8, 8) == 0);

TEST(SizeLimitsTest, RejectsSizesBeforeNarrowing) {
  EXPECT_THROW(
      CheckedByteCount(size_t{kMaxDocumentBytes} + 1, kMaxDocumentBytes),
      std::length_error);
  EXPECT_THROW(
      CheckedByteCount(std::numeric_limits<size_t>::max(), kMaxDocumentBytes),
      std::length_error);
}

TEST(SizeLimitsTest, GrowthCannotOverflowOrExceedLimit) {
  EXPECT_EQ(CheckedByteGrowth(kMaxDocumentBytes, 0, kMaxDocumentBytes),
            kMaxDocumentBytes);
  EXPECT_THROW(CheckedByteGrowth(kMaxDocumentBytes, 1, kMaxDocumentBytes),
               std::length_error);
  EXPECT_THROW(CheckedByteGrowth(1, std::numeric_limits<size_t>::max(),
                                 kMaxDocumentBytes),
               std::length_error);
  EXPECT_THROW(CheckedAdd(std::numeric_limits<size_t>::max(), 1,
                          std::numeric_limits<size_t>::max()),
               std::length_error);
  EXPECT_THROW(CheckedAdd(11, 0, 10), std::length_error);
}

TEST(SizeLimitsTest, LineCountRetainsRepresentableExclusiveEnd) {
  constexpr size_t kMaxLines = std::numeric_limits<uint32_t>::max();
  EXPECT_EQ(CheckedAdd(kMaxDocumentBytes, 1, kMaxLines), kMaxStorageBytes);
  EXPECT_THROW(CheckedAdd(kMaxLines, 1, kMaxLines), std::length_error);
}

TEST(SizeLimitsTest, GapGrowthCapsSpareCapacityAndRejectsImpossibleGrowth) {
  EXPECT_EQ(GapGrowth(0, 0, 1), 4u);
  EXPECT_EQ(GapGrowth(32, 0, 64), 64u);
  EXPECT_EQ(GapGrowth(kMaxStorageBytes - 4, 0, 3), 4u);
  EXPECT_EQ(GapGrowth(kMaxStorageBytes, 8, 8), 0u);
  EXPECT_THROW(GapGrowth(kMaxStorageBytes - 2, 0, 3), std::length_error);
}

}  // namespace
}  // namespace unplugged::internal
