#include <gtest/gtest.h>

#include "document.h"
#include "document_view.h"
#include "dsl.h"

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

TEST(DocumentTest, ConstructEmpty) {
  Document doc{};

  EXPECT_EQ(doc.Len(), 0);
}

TEST(DocumentTest, ConstructFromVectorTransfersContent) {
  std::vector<Byte> source{'a', 'b', '\n', 'c', 'd', '\n', 'e'};
  Document doc{std::vector<Byte>(source)};

  for (ByteIndex i = 0; i < source.size(); ++i) {
    EXPECT_EQ(doc.At(i), source[i]);
  }
}

TEST(DocumentTest, ConstructFromVectorCalculatesLines) {
  std::vector<Byte> source{'a', 'b', '\n', 'c', 'd', '\n', 'e'};
  Document doc{std::vector<Byte>(source)};

  EXPECT_EQ(doc.NumLines(), 3);

  EXPECT_EQ(doc.IsValidLineNumber(0), true);
  EXPECT_EQ(doc.IsValidLineNumber(1), true);
  EXPECT_EQ(doc.IsValidLineNumber(2), true);
  EXPECT_EQ(doc.IsValidLineNumber(3), false);

  EXPECT_EQ(doc.StartOfLine(0), 0);
  EXPECT_EQ(doc.StartOfLine(1), 3);
  EXPECT_EQ(doc.StartOfLine(2), 6);
}

TEST(DocumentTest, LineFromPos) {
  std::vector<Byte> source{'a', 'b', '\n', 'c', 'd', '\n', 'e'};
  Document doc{std::vector<Byte>(source)};

  EXPECT_EQ(doc.LineFromPos(0), 0);
  EXPECT_EQ(doc.LineFromPos(1), 0);
  EXPECT_EQ(doc.LineFromPos(2), 0);
  EXPECT_EQ(doc.LineFromPos(3), 1);
  EXPECT_EQ(doc.LineFromPos(4), 1);
  EXPECT_EQ(doc.LineFromPos(5), 1);
  EXPECT_EQ(doc.LineFromPos(6), 2);
}

TEST(DocumentTest, LineFromPosThrowsWhenLineStartsIsEmpty) {
  Document doc{};

#ifdef CONTRACT_EXCEPTIONS
  ExpectPreconditionViolation([&] { std::ignore = doc.LineFromPos(0); });
#else
  EXPECT_DEATH({ std::ignore = doc.LineFromPos(0); }, "PRECONDITION FAILED");
#endif
}

TEST(DocumentTest, EditInserts) {
  std::vector<Byte> source{'a', 'b', '\n', 'c', 'd', '\n', 'e'};
  Document doc{std::vector<Byte>(source)};

  std::vector<Byte> insert_bytes{'x', 'y', '\n'};
  doc.Edit(2, 0, insert_bytes);

  // Should grow by the size of the inserted bytes.
  EXPECT_EQ(doc.Len(), source.size() + insert_bytes.size());

  // Should update the line starts.
  EXPECT_EQ(doc.NumLines(), 4);
  EXPECT_EQ(doc.StartOfLine(0), 0);
  EXPECT_EQ(doc.StartOfLine(1), 5);
  EXPECT_EQ(doc.StartOfLine(2), 6);
  EXPECT_EQ(doc.StartOfLine(3), 9);

  // Should now be "abxy\n\ncd\ne"
  EXPECT_EQ(doc.At(0), 'a');
  EXPECT_EQ(doc.At(1), 'b');
  EXPECT_EQ(doc.At(2), 'x');
  EXPECT_EQ(doc.At(3), 'y');
  EXPECT_EQ(doc.At(4), '\n');
  EXPECT_EQ(doc.At(5), '\n');
  EXPECT_EQ(doc.At(6), 'c');
  EXPECT_EQ(doc.At(7), 'd');
  EXPECT_EQ(doc.At(8), '\n');
  EXPECT_EQ(doc.At(9), 'e');
}

TEST(DocumentTest, EditDeletes) {
  std::vector<Byte> source{'a', 'b', '\n', 'c', 'd', '\n', 'e'};
  Document doc{std::vector<Byte>(source)};

  doc.Edit(2, 3, {});

  // Should shrink by the size of the deleted bytes.
  EXPECT_EQ(doc.Len(), source.size() - 3);

  // Should update the line starts.
  EXPECT_EQ(doc.NumLines(), 2);
  EXPECT_EQ(doc.StartOfLine(0), 0);
  EXPECT_EQ(doc.StartOfLine(1), 3);

  // Should now be "ab\ne"
  EXPECT_EQ(doc.At(0), 'a');
  EXPECT_EQ(doc.At(1), 'b');
  EXPECT_EQ(doc.At(2), '\n');
  EXPECT_EQ(doc.At(3), 'e');
}

TEST(DocumentTest, EditDeletesFromZero) {
  std::vector<Byte> source{'a', 'b', '\n', 'c', 'd', '\n', 'e'};
  Document doc{std::vector<Byte>(source)};

  EXPECT_EQ(doc.NumLines(), 3);
  EXPECT_EQ(doc.StartOfLine(0), 0);
  EXPECT_EQ(doc.StartOfLine(1), 3);
  EXPECT_EQ(doc.StartOfLine(2), 6);

  doc.Edit(0, 1, {});

  // Should shrink by the size of the deleted bytes.
  EXPECT_EQ(doc.Len(), source.size() - 1);

  // Should update the line starts.
  EXPECT_EQ(doc.NumLines(), 3);
  EXPECT_EQ(doc.StartOfLine(0), 0);
  EXPECT_EQ(doc.StartOfLine(1), 2);
  EXPECT_EQ(doc.StartOfLine(2), 5);

  // Should now be "b\ncd\ne"
  EXPECT_EQ(doc.At(0), 'b');
  EXPECT_EQ(doc.At(1), '\n');
  EXPECT_EQ(doc.At(2), 'c');
  EXPECT_EQ(doc.At(3), 'd');
  EXPECT_EQ(doc.At(4), '\n');
  EXPECT_EQ(doc.At(5), 'e');
}

TEST(DocumentTest, EditDeleteZeroBytesIsNoOp) {
  std::vector<Byte> source{'a', 'b', '\n', 'c', 'd', '\n', 'e'};
  Document doc{std::vector<Byte>(source)};

  doc.Edit(0, 0, {});
  EXPECT_EQ(doc.Len(), source.size());
  EXPECT_EQ(doc.NumLines(), 3);
  EXPECT_EQ(doc.StartOfLine(0), 0);
  EXPECT_EQ(doc.StartOfLine(1), 3);
  EXPECT_EQ(doc.StartOfLine(2), 6);
  EXPECT_EQ(doc.At(0), 'a');
  EXPECT_EQ(doc.At(1), 'b');
  EXPECT_EQ(doc.At(2), '\n');

  doc.Edit(3, 0, {});
  EXPECT_EQ(doc.Len(), source.size());
  EXPECT_EQ(doc.NumLines(), 3);
  EXPECT_EQ(doc.StartOfLine(0), 0);
  EXPECT_EQ(doc.StartOfLine(1), 3);
  EXPECT_EQ(doc.StartOfLine(2), 6);
  EXPECT_EQ(doc.At(3), 'c');

  doc.Edit(2, 0, {});
  EXPECT_EQ(doc.Len(), source.size());
  EXPECT_EQ(doc.NumLines(), 3);
  EXPECT_EQ(doc.StartOfLine(0), 0);
  EXPECT_EQ(doc.StartOfLine(1), 3);
  EXPECT_EQ(doc.StartOfLine(2), 6);
  EXPECT_EQ(doc.At(2), '\n');
}

TEST(DocumentTests, EditDeletesAcrossLineBoundaries) {
  std::vector<Byte> source{'a', 'b', '\n', 'c', 'd', '\n', 'e'};
  Document doc{std::vector<Byte>(source)};

  doc.Edit(2, 1, {});

  // Should shrink by the size of the deleted bytes.
  EXPECT_EQ(doc.Len(), source.size() - 1);

  // Should update the line starts.
  EXPECT_EQ(doc.NumLines(), 2);
  EXPECT_EQ(doc.StartOfLine(0), 0);
  EXPECT_EQ(doc.StartOfLine(1), 5);

  // Should now be "abcd\ne"
  EXPECT_EQ(doc.At(0), 'a');
  EXPECT_EQ(doc.At(1), 'b');
  EXPECT_EQ(doc.At(2), 'c');
  EXPECT_EQ(doc.At(3), 'd');
  EXPECT_EQ(doc.At(4), '\n');
  EXPECT_EQ(doc.At(5), 'e');
}

TEST(DocumentTest, EditReplaces) {
  std::vector<Byte> source{'a', 'b', '\n', 'c', 'd', '\n', 'e'};
  Document doc{std::vector<Byte>(source)};

  std::vector<Byte> insert_bytes{'x', 'y', '\n'};
  doc.Edit(2, 3, insert_bytes);

  // Should remain the same size.
  EXPECT_EQ(doc.Len(), source.size());

  // Should update the line starts.
  EXPECT_EQ(doc.NumLines(), 3);
  EXPECT_EQ(doc.StartOfLine(0), 0);
  EXPECT_EQ(doc.StartOfLine(1), 5);
  EXPECT_EQ(doc.StartOfLine(2), 6);

  // Should now be "abxy\n\ne"
  EXPECT_EQ(doc.At(0), 'a');
  EXPECT_EQ(doc.At(1), 'b');
  EXPECT_EQ(doc.At(2), 'x');
  EXPECT_EQ(doc.At(3), 'y');
  EXPECT_EQ(doc.At(4), '\n');
  EXPECT_EQ(doc.At(5), '\n');
  EXPECT_EQ(doc.At(6), 'e');
}

TEST(DocumentTest, EditDeletesBeforeItInserts) {
  std::vector<Byte> source{'a', 'b', '\n', 'c', 'd', '\n', 'e'};
  Document doc{std::vector<Byte>(source)};

  std::vector<Byte> insert_bytes{'x', 'y', '\n'};
  doc.Edit(2, 1, insert_bytes);

  // Should grow by the size of the inserted bytes minus the size of the deleted
  // bytes.
  EXPECT_EQ(doc.Len(), source.size() + insert_bytes.size() - 1);

  // Should now be "abxy\ncd\ne" because we deleted the '\n' at position 2 and
  // inserted "xy\n" in its place.
  EXPECT_EQ(doc.At(0), 'a');
  EXPECT_EQ(doc.At(1), 'b');
  EXPECT_EQ(doc.At(2), 'x');
  EXPECT_EQ(doc.At(3), 'y');
  EXPECT_EQ(doc.At(4), '\n');
  EXPECT_EQ(doc.At(5), 'c');
  EXPECT_EQ(doc.At(6), 'd');
  EXPECT_EQ(doc.At(7), '\n');
  EXPECT_EQ(doc.At(8), 'e');

  // Should update the line starts.
  EXPECT_EQ(doc.NumLines(), 3);
  EXPECT_EQ(doc.StartOfLine(0), 0);
  EXPECT_EQ(doc.StartOfLine(1), 5);
  EXPECT_EQ(doc.StartOfLine(2), 8);
}

TEST(DocumentTest, BackspaceOnEmptyLineDeletesOnlyOneNewline) {
  // Model an enter-enter-backspace scenario around "A\nB".
  Document doc{std::vector<Byte>{'A', '\n', 'B'}};

  // Enter once at start of line containing B: "A\n\nB"
  doc.Edit(2, 0, AsByteSpan("\n"));
  ASSERT_EQ(doc.NumLines(), 3);
  ASSERT_EQ(doc.StartOfLine(0), 0);
  ASSERT_EQ(doc.StartOfLine(1), 2);
  ASSERT_EQ(doc.StartOfLine(2), 3);

  // Backspace once at start of B line should remove only the preceding
  // newline: "A\nB".
  doc.Edit(2, 1, {});

  EXPECT_EQ(doc.NumLines(), 2);
  EXPECT_EQ(doc.StartOfLine(0), 0);
  EXPECT_EQ(doc.StartOfLine(1), 2);
  EXPECT_EQ(doc.Len(), 3);
  EXPECT_EQ(doc.At(0), 'A');
  EXPECT_EQ(doc.At(1), '\n');
  EXPECT_EQ(doc.At(2), 'B');
}

#ifdef CONTRACT_EXCEPTIONS
TEST(DocumentTest, PartialViewRejectsOutOfRangeSlices) {
  Document doc{std::vector<Byte>{'a', 'b', 'c'}};

  ExpectPreconditionViolation([&] { std::ignore = doc.PartialView(4, 0); });
  ExpectPreconditionViolation([&] { std::ignore = doc.PartialView(2, 2); });
}
#endif
