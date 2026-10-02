#include "document.h"

#include "document_view.h"
#include "logical_line_range.h"

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

Document::Document(std::vector<Byte> &&buffer)
    : buffer_(std::move(buffer)), line_starts_(FindLineStarts(buffer_)) {}

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

  buffer_.Delete(pos, delete_count);
  line_starts_.UpdateOnDelete(pos, delete_count);

  buffer_.Insert(pos, insert_bytes);
  line_starts_.UpdateOnInsert(pos, insert_bytes);
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
