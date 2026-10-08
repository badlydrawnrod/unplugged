#include <gtest/gtest.h>

#include <new>
#include <string>
#include <vector>

#include "document/document.h"
#include "document/document_view.h"
#include "document/internal/gap_buffer/gap_buffer.h"
#include "document/internal/line_starts/line_starts.h"
#include "internal/test_support/allocation_failure.h"

using unplugged::testing::FailAllocationAfter;

namespace {
template <typename Make, typename Edit, typename VerifyBefore,
          typename VerifyAfter>
void ExerciseAllocationFailures(Make make, Edit edit,
                                VerifyBefore verify_before,
                                VerifyAfter verify_after) {
  size_t failures = 0;
  // Discover allocation points rather than asserting their number or order.
  for (size_t fail_after = 0;; ++fail_after) {
    auto value = make();
    bool failed = false;
    try {
      FailAllocationAfter inject_failure(fail_after);
      edit(value);
    } catch (const std::bad_alloc&) {
      failed = true;
    }
    if (!failed) {
      verify_after(value);
      EXPECT_GT(failures, 0u);
      return;
    }
    ++failures;
    verify_before(value);
    // The same object must still support editing once allocation is available.
    edit(value);
    verify_after(value);
  }
}

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

std::string Snapshot(const GapBuffer& buffer) {
  std::vector<Byte> bytes;
  EXPECT_EQ(buffer.AppendRange(0, buffer.Len(), bytes), buffer.Len());
  return std::string(bytes.begin(), bytes.end());
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
