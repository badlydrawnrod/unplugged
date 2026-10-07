#include <gtest/gtest.h>

#include "dsl.h"
#include "line_starts.h"

#ifdef CONTRACT_EXCEPTIONS
template <typename F>
void ExpectPreconditionViolation(F &&fn) {
  try {
    fn();
    FAIL() << "Expected std::logic_error";
  } catch (const std::logic_error &e) {
    EXPECT_NE(std::string(e.what()).find("PRECONDITION"), std::string::npos);
  }
}
#endif

TEST(LineStartsTest, ConstructFromVectorTransfersContent) {
  std::vector<ByteIndex> source{0, 5, 10};
  LineStarts view{std::move(source)};

  EXPECT_EQ(view.NumLines(), 3);
  EXPECT_EQ(view.StartOfLine(0), 0);
  EXPECT_EQ(view.StartOfLine(1), 5);
  EXPECT_EQ(view.StartOfLine(2), 10);
}

TEST(LineStartsTest, RejectsUnrepresentableDocumentOffsetsOnConstruction) {
  EXPECT_THROW((LineStarts{std::vector<ByteIndex>{0, kMaxDocumentBytes + 1}}),
               std::length_error);
}

TEST(LineStartsTest, RejectsGrowthBeforeChangingExistingStarts) {
  LineStarts lines{std::vector<ByteIndex>{0, kMaxDocumentBytes}};
  EXPECT_THROW(lines.UpdateOnInsert(0, AsByteSpan("x")), std::length_error);
  ASSERT_EQ(lines.NumLines(), 2u);
  EXPECT_EQ(lines.StartOfLine(0), 0u);
  EXPECT_EQ(lines.StartOfLine(1), kMaxDocumentBytes);
}

TEST(LineStartsTest, NewLineStartCanReachButNotExceedSizeLimit) {
  LineStarts lines{std::vector<ByteIndex>{0}};
  lines.UpdateOnInsert(kMaxDocumentBytes - 1, AsByteSpan("\n"));
  ASSERT_EQ(lines.NumLines(), 2u);
  EXPECT_EQ(lines.StartOfLine(1), kMaxDocumentBytes);

  EXPECT_THROW(lines.UpdateOnInsert(kMaxDocumentBytes, AsByteSpan("\n")),
               std::length_error);
  ASSERT_EQ(lines.NumLines(), 2u);
  EXPECT_EQ(lines.StartOfLine(1), kMaxDocumentBytes);
}

TEST(LineStartsTest, DeletionEndCannotWrapOrExceedSizeLimit) {
  LineStarts lines{std::vector<ByteIndex>{0, kMaxDocumentBytes}};
  EXPECT_THROW(lines.UpdateOnDelete(kMaxDocumentBytes, 1), std::length_error);
  EXPECT_THROW(lines.UpdateOnDelete(kMaxDocumentBytes, kMaxDocumentBytes),
               std::length_error);
  ASSERT_EQ(lines.NumLines(), 2u);
  EXPECT_EQ(lines.StartOfLine(1), kMaxDocumentBytes);

  lines.UpdateOnDelete(kMaxDocumentBytes - 1, 1);
  EXPECT_EQ(lines.NumLines(), 1u);
  EXPECT_EQ(lines.StartOfLine(0), 0u);
}

TEST(LineStartsTest, UpdateOnInsertShiftsExistingLineStartsAndAddsNew) {
  LineStarts view{std::vector<ByteIndex>{0, 5, 10}};

  // Insert "abc\ndef\nghi" (11 bytes) at position 4. This should shift the line
  // starts at 5 and 10 to 16 and 21 respectively, and add new line starts at
  // 8 and 12 (the positions after the newlines).
  view.UpdateOnInsert(4, AsByteSpan("abc\ndef\nghi"));

  EXPECT_EQ(view.NumLines(), 5);
  EXPECT_EQ(view.StartOfLine(0), 0);   // Unchanged existing line start.
  EXPECT_EQ(view.StartOfLine(1), 8);   // Newly inserted line start.
  EXPECT_EQ(view.StartOfLine(2), 12);  // Newly inserted line start.
  EXPECT_EQ(view.StartOfLine(3), 16);  // Shifted existing line start (was 5).
  EXPECT_EQ(view.StartOfLine(4), 21);  // Shifted existing line start (was 10).
}

TEST(LineStartsTest,
     UpdateOnInsertShiftsExistingLineStartsAndAddsNewAtExistingLineStart) {
  LineStarts view{std::vector<ByteIndex>{0, 5, 10}};

  // Insert "abc\ndef\nghi" (11 bytes) at position 5 which is already a line
  // start.
  view.UpdateOnInsert(5, AsByteSpan("abc\ndef\nghi"));

  EXPECT_EQ(view.NumLines(), 5);
  EXPECT_EQ(view.StartOfLine(0), 0);   // Unchanged existing line start.
  EXPECT_EQ(view.StartOfLine(1), 5);   // Newly inserted line start.
  EXPECT_EQ(view.StartOfLine(2), 9);   // Newly inserted line start.
  EXPECT_EQ(view.StartOfLine(3), 13);  // Newly inserted line start.
  EXPECT_EQ(view.StartOfLine(4), 21);  // Shifted existing line start (was 10).
}

TEST(LineStartsTest,
     UpdateOnDeleteRemovesAffectedLineStartsAndShiftsRemaining) {
  LineStarts view{std::vector<ByteIndex>{0, 5, 10}};

  view.UpdateOnDelete(2, 4);

  EXPECT_EQ(view.NumLines(), 2);
  EXPECT_EQ(view.StartOfLine(0), 0);
  EXPECT_EQ(view.StartOfLine(1), 6);
}

TEST(LineStartsTest, UpdateOnDeleteRemovesLineStartsAndShiftsExisting) {
  LineStarts view{std::vector<ByteIndex>{0, 5, 10, 15, 20}};

  // Delete 7 bytes starting at position 4. This should remove the line starts
  // at 5 and 10, and shift the line starts at 15 and 20 to 8 and 13
  // respectively.
  view.UpdateOnDelete(4, 7);

  EXPECT_EQ(view.NumLines(), 3);
  EXPECT_EQ(view.StartOfLine(0), 0);   // Unchanged existing line start.
  EXPECT_EQ(view.StartOfLine(1), 8);   // Shifted existing line start (was 15).
  EXPECT_EQ(view.StartOfLine(2), 13);  // Shifted existing line start (was 20).
}

TEST(LineStartsTest, UpdateOnDeleteNoOpWhenDeleteCountIsZero) {
  LineStarts view{std::vector<ByteIndex>{0, 5, 10}};

  view.UpdateOnDelete(0, 0);
  EXPECT_EQ(view.NumLines(), 3);
  EXPECT_EQ(view.StartOfLine(0), 0);
  EXPECT_EQ(view.StartOfLine(1), 5);
  EXPECT_EQ(view.StartOfLine(2), 10);

  view.UpdateOnDelete(5, 0);
  EXPECT_EQ(view.NumLines(), 3);
  EXPECT_EQ(view.StartOfLine(0), 0);
  EXPECT_EQ(view.StartOfLine(1), 5);
  EXPECT_EQ(view.StartOfLine(2), 10);

  view.UpdateOnDelete(2, 0);
  EXPECT_EQ(view.NumLines(), 3);
  EXPECT_EQ(view.StartOfLine(0), 0);
  EXPECT_EQ(view.StartOfLine(1), 5);
  EXPECT_EQ(view.StartOfLine(2), 10);
}

TEST(LineStartsTest, LineFromPosReturnsCorrectLineNumberWhenLineNumberIsValid) {
  LineStarts view{std::vector<ByteIndex>{0, 5, 10, 15}};

  EXPECT_EQ(view.LineFromPos(0), 0);
  EXPECT_EQ(view.LineFromPos(4), 0);
  EXPECT_EQ(view.LineFromPos(5), 1);
  EXPECT_EQ(view.LineFromPos(9), 1);
  EXPECT_EQ(view.LineFromPos(10), 2);
  EXPECT_EQ(view.LineFromPos(14), 2);
  EXPECT_EQ(view.LineFromPos(15), 3);
}

TEST(LineStartsTest,
     LineFromPosReturnsLastLineNumberWhenPosIsBeyondLastLineStart) {
  LineStarts view{std::vector<ByteIndex>{0, 5, 10, 15}};

  EXPECT_EQ(view.LineFromPos(16), 3);
  EXPECT_EQ(view.LineFromPos(20), 3);
}

TEST(LineStartsTest, LineFromPosThrowsWhenLineStartsIsEmpty) {
  LineStarts view{std::vector<ByteIndex>{}};

#ifdef CONTRACT_EXCEPTIONS
  ExpectPreconditionViolation([&] { std::ignore = view.LineFromPos(0); });
#else
  EXPECT_DEATH({ std::ignore = view.LineFromPos(0); }, "PRECONDITION FAILED");
#endif
}

TEST(LineStartsTest, DeleteAcrossLineBoundaries) {
  {
    LineStarts view{std::vector<ByteIndex>{0, 3, 6}};
    view.UpdateOnDelete(2, 1);
    EXPECT_EQ(view.NumLines(), 2);
    EXPECT_EQ(view.StartOfLine(0), 0);
    EXPECT_EQ(view.StartOfLine(1), 5);
  }
  {
    LineStarts view{std::vector<ByteIndex>{0, 3, 6, 9}};
    // Starts [0,3,6,9] correspond to newlines at byte positions 2,5,8.
    // Deleting [2,6) crosses two line boundaries (removes the newline at 2
    // and the newline at 5), so starts 3 and 6 are removed and 9 shifts to 5.
    view.UpdateOnDelete(2, 4);
    EXPECT_EQ(view.NumLines(), 2);
    EXPECT_EQ(view.StartOfLine(0), 0);
    EXPECT_EQ(view.StartOfLine(1), 5);
  }
}

TEST(LineStartsTest, DeleteZeroBytesDoesNothing) {
  LineStarts view{std::vector<ByteIndex>{0, 3, 6}};
  view.UpdateOnDelete(2, 0);
  EXPECT_EQ(view.NumLines(), 3);
  EXPECT_EQ(view.StartOfLine(0), 0);
  EXPECT_EQ(view.StartOfLine(1), 3);
  EXPECT_EQ(view.StartOfLine(2), 6);
}

TEST(LineStartsTest, DeleteFromZero) {
  LineStarts view{std::vector<ByteIndex>{0, 3, 6}};
  view.UpdateOnDelete(0, 1);
  EXPECT_EQ(view.NumLines(), 3);
  EXPECT_EQ(view.StartOfLine(0), 0);
  EXPECT_EQ(view.StartOfLine(1), 2);
  EXPECT_EQ(view.StartOfLine(2), 5);
}

TEST(LineStartsTest,
     DeleteSingleNewlineFromConsecutiveNewlinesPreservesNextLine) {
  // Buffer shape: "A\n\nB". Line starts at [0, 2, 3]. Deleting the second
  // newline at pos=2 should result in "A\nB" with line starts [0, 2].
  LineStarts view{std::vector<ByteIndex>{0, 2, 3}};
  view.UpdateOnDelete(2, 1);

  EXPECT_EQ(view.NumLines(), 2);
  EXPECT_EQ(view.StartOfLine(0), 0);
  EXPECT_EQ(view.StartOfLine(1), 2);
}
