#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "document_view.h"
#include "dsl.h"

TEST(DocumentViewTest, FullViewIteratesInLogicalOrder) {
  Document doc{std::vector<Byte>{'a', 'b', '\n', 'c', 'd'}};

  auto view = doc.View();

  EXPECT_EQ(view.StartIndex(), 0);
  EXPECT_EQ(view.size(), 5u);
  EXPECT_FALSE(view.empty());

  std::vector<Byte> out;
  for (Byte b : view) {
    out.push_back(b);
  }

  EXPECT_EQ(out, (std::vector<Byte>{'a', 'b', '\n', 'c', 'd'}));
}

TEST(DocumentViewTest, PartialViewStartsAtTheCorrectDocumentIndex) {
  Document doc{std::vector<Byte>{'a', 'b', '\n', 'c', 'd'}};

  auto view = doc.PartialView(2, 3);

  EXPECT_EQ(view.StartIndex(), 2);
  EXPECT_EQ(view.size(), 3u);
  EXPECT_EQ(view[0], '\n');
  EXPECT_EQ(view[1], 'c');
  EXPECT_EQ(view[2], 'd');
}

TEST(DocumentViewTest, IteratorMapsBackToDocumentIndex) {
  Document doc{std::vector<Byte>{'a', 'b', 'c', 'd'}};

  auto view = doc.PartialView(1, 3);
  auto it = view.begin();

  EXPECT_EQ(it.DocumentIndex(), 1);
  EXPECT_EQ(*it, 'b');

  ++it;
  EXPECT_EQ(it.DocumentIndex(), 2);
  EXPECT_EQ(*it, 'c');

  it += 2;
  EXPECT_EQ(it.DocumentIndex(), 4);
  EXPECT_TRUE(it == view.end());
}

TEST(DocumentViewTest, EmptyViewHasNoElements) {
  Document doc{std::vector<Byte>{'a', 'b'}};

  auto view = doc.PartialView(1, 0);

  EXPECT_TRUE(view.empty());
  EXPECT_EQ(view.size(), 0u);
  EXPECT_EQ(view.begin(), view.end());
}

TEST(DocumentViewTest, SearchWithinViewFindsLogicalMatch) {
  Document doc{std::vector<Byte>{'x', 'a', 'b', 'a', 'y'}};

  auto view = doc.PartialView(1, 3);
  auto found = std::find(view.begin(), view.end(), 'b');

  ASSERT_NE(found, view.end());
  EXPECT_EQ(found.DocumentIndex(), 2);
  EXPECT_EQ(*found, 'b');
}

TEST(DocumentViewTest, PlaysWellWithRanges) {
  Document doc{std::vector<Byte>{'a', 'b', 'c', 'd'}};

  auto view = doc.View();
  auto found = std::ranges::find(view, 'c');
  ASSERT_NE(found, view.end());
  EXPECT_EQ(*found, 'c');
}
