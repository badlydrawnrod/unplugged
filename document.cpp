#include "document.h"

#include <type_traits>

#include "document_view.h"
#include "internal/size_limits.h"
#include "logical_line_range.h"

static_assert(std::is_nothrow_move_assignable_v<LineStarts>,
              "committing a prepared line index must not throw");

[[nodiscard]] unplugged::dbc::InvariantResult Document::check_invariants()
    const {
  auto buffer_invariants = buffer_.check_invariants();
  if (buffer_invariants) {
    return buffer_invariants;
  }

  auto line_starts_invariants = line_starts_.check_invariants();
  if (line_starts_invariants) {
    return line_starts_invariants;
  }

#if !defined(NDEBUG)
  // This full scan is only for contract-enabled builds. Use storage directly:
  // document views invoke the document invariant guard themselves.
  const size_t num_lines = line_starts_.NumLines();
  DBC_INVARIANT(num_lines > 0, "a document must contain its initial line");
  DBC_INVARIANT(line_starts_.StartOfLine(0) == 0,
                "the initial document line must start at byte zero");

  size_t next_line = 1;
  const ByteCount len = buffer_.Len();
  for (ByteIndex pos = 0; pos < len; ++pos) {
    if (buffer_.At(pos) != '\n') {
      continue;
    }
    DBC_INVARIANT(next_line < num_lines,
                  "newline at byte {} has no following line start", pos);
    const ByteIndex start =
        line_starts_.StartOfLine(static_cast<LineNumber>(next_line));
    DBC_INVARIANT(start == pos + 1,
                  "line {} must start immediately after newline at byte {}; "
                  "actual start={}",
                  next_line, pos, start);
    ++next_line;
  }
  DBC_INVARIANT(next_line == num_lines,
                "line index contains entries without corresponding newlines; "
                "expected {} lines, actual={}",
                next_line, num_lines);
#endif

  return {};
}

std::vector<ByteIndex> FindLineStarts(const GapBuffer &buffer) {
  std::vector<ByteIndex> line_starts;
  line_starts.push_back(0);  // The first line always starts at position 0.

  const ByteCount len = buffer.Len();
  for (ByteIndex pos = 0; pos < len; ++pos) {
    if (buffer.At(pos) == '\n') {
      line_starts.push_back(pos + 1);
    }
  }

  return line_starts;
}

Document::Document() : Document(std::vector<Byte>{}) {}

Document::Document(std::vector<Byte> &&buffer)
    : buffer_(std::move(buffer)), line_starts_(FindLineStarts(buffer_)) {
  DBC_GUARD_CLASS_INVARIANTS();
}

ByteCount Document::Len() const noexcept { return buffer_.Len(); }

Byte Document::At(ByteIndex pos) const
    noexcept(!kDocumentContractExceptionsEnabled) {
  return buffer_.At(pos);
}

size_t Document::NumLines() const noexcept { return line_starts_.NumLines(); }

ByteIndex Document::StartOfLine(LineStarts::LineNumber line_number) const
    noexcept(!kDocumentContractExceptionsEnabled) {
  return line_starts_.StartOfLine(line_number);
}

bool Document::IsValidLineNumber(
    LineStarts::LineNumber line_number) const noexcept {
  return line_starts_.IsValidLineNumber(line_number);
}

LineStarts::LineNumber Document::LineFromPos(ByteIndex pos) const
    noexcept(!kDocumentContractExceptionsEnabled) {
  return line_starts_.LineFromPos(pos);
}

void Document::Edit(ByteIndex pos, ByteCount delete_count,
                    const ByteSpan insert_bytes) {
  DBC_GUARD_CLASS_INVARIANTS();

  DBC_PRE(pos <= Len(),
          "position cannot exceed document logical length. pos={}, Len()={}",
          pos, Len());
  DBC_PRE(
      delete_count <= Len() - pos,
      "cannot delete beyond the end of the document. pos={}, delete_count={}, "
      "Len()={}",
      pos, delete_count, Len());

  // Reject oversized replacements before either storage or index is mutated.
  unplugged::internal::CheckedByteGrowth(
      Len() - delete_count, insert_bytes.size(), kMaxDocumentBytes);

  // These operations do not allocate, and the validated deletion range cannot
  // fail the line-index size checks. Preserve allocation-free deletion/no-op.
  if (insert_bytes.empty()) {
    buffer_.Delete(pos, delete_count);
    line_starts_.UpdateOnDelete(pos, delete_count);
    return;
  }

  // Prepare the index independently. Until byte replacement succeeds, both
  // observable document components remain unchanged if an allocation fails.
  auto next_line_starts = line_starts_;
  next_line_starts.UpdateOnDelete(pos, delete_count);
  next_line_starts.UpdateOnInsert(pos, insert_bytes);

  buffer_.Replace(pos, delete_count, insert_bytes);
  line_starts_ = std::move(next_line_starts);
}

DocumentView Document::View() const noexcept {
  return PartialView(0, Len());
}

DocumentView Document::PartialView(ByteIndex start, ByteCount count) const
    noexcept(!kDocumentContractExceptionsEnabled) {
  DBC_GUARD_CLASS_INVARIANTS();

  DBC_PRE(
      start <= Len(),
      "view start cannot exceed document logical length. start={}, Len()={}",
      start, Len());
  DBC_PRE(count <= Len() - start,
          "view count cannot extend past the end of the document. start={}, "
          "count={}, Len()={}",
          start, count, Len());

  return DocumentView{*this, start, count};
}

LogicalLineRange Document::Lines() const noexcept {
  return LogicalLineRange{*this, 0, static_cast<LineNumber>(NumLines())};
}

LogicalLineRange Document::LinesFrom(LineNumber first_line) const
    noexcept(!kDocumentContractExceptionsEnabled) {
  const auto num_lines = static_cast<LineNumber>(NumLines());
  const LineNumber clamped_first = std::min(first_line, num_lines);
  return LogicalLineRange{*this, clamped_first, num_lines};
}
