#include <gtest/gtest.h>

#include <vector>

#include "document.h"
#include "document_view.h"
#include "dsl.h"
#include "wrapped_row_range.h"

#ifdef CONTRACT_EXCEPTIONS
template <typename F>
void ExpectPreconditionViolation(F&& fn) {
  try {
    fn();
    FAIL() << "Expected std::logic_error";
  } catch (const std::logic_error& e) {
    EXPECT_NE(std::string(e.what()).find("PRECONDITION"), std::string::npos);
  }
}
#endif

std::vector<Byte> Collect(DocumentView view) {
  std::vector<Byte> out;
  for (Byte b : view) {
    out.push_back(b);
  }
  return out;
}

TEST(WrappedRowRangeTest, StripsTrailingNewlineFromRows) {
  Document doc{std::vector<Byte>{'a', 'b', '\n'}};
  auto line = *doc.LinesFrom(0).begin();

  WrappedRowRange rows(doc, line, 10);

  ASSERT_EQ(rows.row_count(), 1u);
  auto it = rows.begin();
  ASSERT_NE(it, rows.end());
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'a', 'b'}));
}

TEST(WrappedRowRangeTest, EmptyLineProducesSingleEmptyRow) {
  Document doc{std::vector<Byte>{'\n'}};
  auto line = *doc.LinesFrom(0).begin();

  WrappedRowRange rows(doc, line, 10);

  ASSERT_EQ(rows.row_count(), 1u);
  auto it = rows.begin();
  ASSERT_NE(it, rows.end());
  EXPECT_TRUE((*it).empty());
  ++it;
  EXPECT_EQ(it, rows.end());
}

TEST(WrappedRowRangeTest, WrapsLongLineAcrossRows) {
  Document doc{std::vector<Byte>{'a', 'b', 'c', 'd', 'e', 'f', '\n'}};
  auto line = *doc.LinesFrom(0).begin();

  WrappedRowRange rows(doc, line, 4);

  ASSERT_EQ(rows.row_count(), 2u);
  auto it = rows.begin();
  ASSERT_NE(it, rows.end());
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'a', 'b', 'c', 'd'}));

  ++it;
  ASSERT_NE(it, rows.end());
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'e', 'f'}));

  ++it;
  EXPECT_EQ(it, rows.end());
}

TEST(WrappedRowRangeTest, LastLineWithoutNewlineStillWraps) {
  Document doc{std::vector<Byte>{'a', '\n', 'b', 'c', 'd'}};
  auto lines = doc.LinesFrom(0);
  auto it_line = lines.begin();
  ++it_line;
  auto line = *it_line;

  WrappedRowRange rows(doc, line, 2);

  ASSERT_EQ(rows.row_count(), 2u);
  auto it = rows.begin();
  ASSERT_NE(it, rows.end());
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'b', 'c'}));

  ++it;
  ASSERT_NE(it, rows.end());
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'d'}));
}

TEST(WrappedRowRangeTest, WidthOneProducesOneBytePerRow) {
  Document doc{std::vector<Byte>{'x', 'y', 'z', '\n'}};
  auto line = *doc.LinesFrom(0).begin();

  WrappedRowRange rows(doc, line, 1);

  ASSERT_EQ(rows.row_count(), 3u);
  auto it = rows.begin();
  ASSERT_NE(it, rows.end());
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'x'}));
  ++it;
  ASSERT_NE(it, rows.end());
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'y'}));
  ++it;
  ASSERT_NE(it, rows.end());
  EXPECT_EQ(Collect(*it), (std::vector<Byte>{'z'}));
  ++it;
  EXPECT_EQ(it, rows.end());
}

TEST(WrappedRowRangeTest, RejectsZeroWidth) {
  Document doc{std::vector<Byte>{'a', '\n'}};
  auto line = *doc.LinesFrom(0).begin();

#ifdef CONTRACT_EXCEPTIONS
  ExpectPreconditionViolation([&] { std::ignore = WrappedRowRange(doc, line, 0); });
#else
  EXPECT_DEATH({ std::ignore = WrappedRowRange(doc, line, 0); },
               "PRECONDITION FAILED");
#endif
}
