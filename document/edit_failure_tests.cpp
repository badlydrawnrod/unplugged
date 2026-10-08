#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "document/document.h"
#include "document/document_view.h"
#include "test_support/allocation_failure.h"

using unplugged::testing::ExerciseAllocationFailures;
using unplugged::testing::FailAllocationAfter;

namespace {
void ExpectDocument(const Document& doc, const std::string& text,
                    const std::vector<ByteIndex>& starts) {
  const auto view = doc.View();
  EXPECT_EQ(std::string(view.begin(), view.end()), text);
  ASSERT_EQ(doc.NumLines(), starts.size());
  for (Document::LineNumber line = 0; line < starts.size(); ++line) {
    EXPECT_EQ(doc.StartOfLine(line), starts[line]);
  }
  EXPECT_FALSE(doc.CheckInvariants().has_value());
}

}  // namespace

// Feature: document/features/document_edit_failures.feature
// Scenario: An allocation failure leaves a replacement unapplied
TEST(EditFailureTest, DocumentReplacementPreservesContentAndLinesOnFailure) {
  const std::string inserted = "x\ny\nz\n";
  DocumentView existing_view;
  ExerciseAllocationFailures(
      [] { return Document{std::vector<Byte>{'a', '\n', 'b', '\n', 'c'}}; },
      [&](Document& doc) {
        existing_view = doc.View();
        doc.Edit(1, 3, AsByteSpan(inserted));
      },
      [&](const Document& doc) {
        ExpectDocument(doc, "a\nb\nc", {0, 2, 4});
        EXPECT_EQ(std::string(existing_view.begin(), existing_view.end()),
                  "a\nb\nc");
      },
      [](const Document& doc) {
        ExpectDocument(doc, "ax\ny\nz\nc", {0, 3, 5, 7});
      });
}

// Feature: document/features/document_edit_failures.feature
// Scenario: Deletion succeeds when allocation is unavailable
TEST(EditFailureTest, DeletionAndNoOpDoNotNeedAllocation) {
  Document doc{std::vector<Byte>{'a', '\n', 'b', '\n', 'c'}};
  {
    FailAllocationAfter inject_failure(0);
    doc.Edit(2, 0, {});
  }
  ExpectDocument(doc, "a\nb\nc", {0, 2, 4});
  {
    FailAllocationAfter inject_failure(0);
    doc.Edit(1, 3, {});
  }
  ExpectDocument(doc, "ac", {0});
}
