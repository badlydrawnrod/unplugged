#include <gtest/gtest.h>

#include <vector>

#include "document/document.h"
#include "document/document_view.h"

std::vector<Byte> Collect(DocumentView view) {
  std::vector<Byte> out;
  for (Byte b : view) {
    out.push_back(b);
  }
  return out;
}

TEST(LogicalLineRangeTest, EmptyDocumentHasOneEmptyLine) {
  Document doc{};

  auto lines = doc.Lines();

  EXPECT_FALSE(lines.empty());
  EXPECT_EQ(lines.NumLines(), 1u);
  auto it = lines.begin();
  ASSERT_NE(it, lines.end());
  EXPECT_EQ(it.GetLineNumber(), 0u);
  EXPECT_TRUE((*it).empty());
  ++it;
  EXPECT_EQ(it, lines.end());

  EXPECT_EQ(doc.LinesFrom(0).NumLines(), 1u);
  EXPECT_TRUE(doc.LinesFrom(1).empty());
  EXPECT_TRUE(doc.LinesFrom(10).empty());
}

TEST(LogicalLineRangeTest, IteratesLogicalLinesAsDocumentViews) {
  Document doc{std::vector<Byte>{'a', 'b', '\n', 'c', '\n', 'd'}};

  auto lines = doc.Lines();
  auto it = lines.begin();

  ASSERT_NE(it, lines.end());
  EXPECT_EQ(it.GetLineNumber(), 0u);
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'a', 'b', '\n'}));

  ++it;
  ASSERT_NE(it, lines.end());
  EXPECT_EQ(it.GetLineNumber(), 1u);
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'c', '\n'}));

  ++it;
  ASSERT_NE(it, lines.end());
  EXPECT_EQ(it.GetLineNumber(), 2u);
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'d'}));

  ++it;
  EXPECT_EQ(it, lines.end());
}

TEST(LogicalLineRangeTest, LinesFromStartsAtRequestedLine) {
  Document doc{std::vector<Byte>{'a', 'b', '\n', 'c', '\n', 'd'}};

  auto lines = doc.LinesFrom(1);
  auto it = lines.begin();

  ASSERT_NE(it, lines.end());
  EXPECT_EQ(it.GetLineNumber(), 1u);
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'c', '\n'}));

  ++it;
  ASSERT_NE(it, lines.end());
  EXPECT_EQ(it.GetLineNumber(), 2u);
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'d'}));

  ++it;
  EXPECT_EQ(it, lines.end());
}

TEST(LogicalLineRangeTest, LinesFromClampsOutOfRangeToEmptyRange) {
  Document doc{std::vector<Byte>{'a', 'b', '\n', 'c', '\n', 'd'}};

  auto lines = doc.LinesFrom(10);

  EXPECT_TRUE(lines.empty());
  EXPECT_EQ(lines.begin(), lines.end());
  EXPECT_EQ(lines.FirstLine(), 3u);
  EXPECT_EQ(lines.EndLineExclusive(), 3u);
}

TEST(LogicalLineRangeTest, LinesFromIncludesRemainingLines) {
  Document doc{std::vector<Byte>{'a', 'b', '\n', 'c', '\n', 'd'}};

  auto lines = doc.LinesFrom(2);

  EXPECT_EQ(lines.FirstLine(), 2u);
  EXPECT_EQ(lines.EndLineExclusive(), 3u);
  EXPECT_EQ(lines.NumLines(), 1u);

  auto it = lines.begin();
  ASSERT_NE(it, lines.end());
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'d'}));
  ++it;
  EXPECT_EQ(it, lines.end());
}
