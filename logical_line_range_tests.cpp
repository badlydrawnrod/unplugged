#include <gtest/gtest.h>

#include <vector>

#include "document.h"
#include "document_view.h"
#include "dsl.h"

std::vector<Byte> Collect(DocumentView view) {
  std::vector<Byte> out;
  for (Byte b : view) {
    out.push_back(b);
  }
  return out;
}

TEST(LogicalLineRangeTest, EmptyDocumentHasNoLines) {
  Document doc{};

  auto lines = doc.Lines();

  EXPECT_TRUE(lines.empty());
  EXPECT_EQ(lines.line_count(), 0u);
  EXPECT_EQ(lines.begin(), lines.end());
}

TEST(LogicalLineRangeTest, IteratesLogicalLinesAsDocumentViews) {
  Document doc{std::vector<Byte>{'a', 'b', '\n', 'c', '\n', 'd'}};

  auto lines = doc.Lines();
  auto it = lines.begin();

  ASSERT_NE(it, lines.end());
  EXPECT_EQ(it.line_number(), 0u);
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'a', 'b', '\n'}));

  ++it;
  ASSERT_NE(it, lines.end());
  EXPECT_EQ(it.line_number(), 1u);
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'c', '\n'}));

  ++it;
  ASSERT_NE(it, lines.end());
  EXPECT_EQ(it.line_number(), 2u);
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'d'}));

  ++it;
  EXPECT_EQ(it, lines.end());
}

TEST(LogicalLineRangeTest, LinesFromStartsAtRequestedLine) {
  Document doc{std::vector<Byte>{'a', 'b', '\n', 'c', '\n', 'd'}};

  auto lines = doc.LinesFrom(1);
  auto it = lines.begin();

  ASSERT_NE(it, lines.end());
  EXPECT_EQ(it.line_number(), 1u);
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'c', '\n'}));

  ++it;
  ASSERT_NE(it, lines.end());
  EXPECT_EQ(it.line_number(), 2u);
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'d'}));

  ++it;
  EXPECT_EQ(it, lines.end());
}

TEST(LogicalLineRangeTest, LinesFromClampsOutOfRangeToEmptyRange) {
  Document doc{std::vector<Byte>{'a', 'b', '\n', 'c', '\n', 'd'}};

  auto lines = doc.LinesFrom(10);

  EXPECT_TRUE(lines.empty());
  EXPECT_EQ(lines.begin(), lines.end());
  EXPECT_EQ(lines.first_line(), 3u);
  EXPECT_EQ(lines.end_line_exclusive(), 3u);
}

TEST(LogicalLineRangeTest, LinesFromIncludesRemainingLines) {
  Document doc{std::vector<Byte>{'a', 'b', '\n', 'c', '\n', 'd'}};

  auto lines = doc.LinesFrom(2);

  EXPECT_EQ(lines.first_line(), 2u);
  EXPECT_EQ(lines.end_line_exclusive(), 3u);
  EXPECT_EQ(lines.line_count(), 1u);

  auto it = lines.begin();
  ASSERT_NE(it, lines.end());
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'d'}));
  ++it;
  EXPECT_EQ(it, lines.end());
}
