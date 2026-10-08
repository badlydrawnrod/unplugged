#pragma once

#include <limits>

#include "gap_buffer.h"
#include "line_starts.h"
#include "document/logical_line_range.h"

#if defined(CONTRACT_EXCEPTIONS)
inline constexpr bool kDocumentContractExceptionsEnabled = true;
#else
inline constexpr bool kDocumentContractExceptionsEnabled = false;
#endif

class DocumentView;

// Ethos: Narrow, deep interfaces. The Document class is a narrow interface that
// provides a deep abstraction over the underlying data structures (GapBuffer
// and LineStarts). It exposes only the necessary operations for manipulating
// the document, while encapsulating the complexity of the underlying data
// structures. This design promotes separation of concerns, making it easier to
// maintain and extend the codebase.
class Document {
 public:
  using LineNumber = LineStarts::LineNumber;
  static_assert(static_cast<std::uintmax_t>(kMaxDocumentBytes) + 1 <=
                std::numeric_limits<LineNumber>::max());

  // Every document has at least one logical line, starting at byte 0. An empty
  // document has one empty line; each newline adds another logical line.
  Document();

  // Throws std::length_error if the byte length exceeds kMaxDocumentBytes.
  Document(std::vector<Byte> &&buffer);

  [[nodiscard]] unplugged::dbc::InvariantResult check_invariants() const;

  // Mutation.
  // Oversized results throw std::length_error before changing the document.
  // Allocation errors propagate; a failed edit leaves bytes and line queries
  // unchanged. Existing views remain usable after a failed edit.
  void Edit(ByteIndex pos, ByteCount delete_count, const ByteSpan insert_bytes);
  // void Undo();
  // void Redo();

  // Storage queries.
  [[nodiscard]] ByteCount Len() const noexcept;
  [[nodiscard]] Byte At(ByteIndex pos) const
      noexcept(!kDocumentContractExceptionsEnabled);
  [[nodiscard]] size_t NumLines() const noexcept;
  [[nodiscard]] ByteIndex StartOfLine(LineStarts::LineNumber line_number) const
      noexcept(!kDocumentContractExceptionsEnabled);
  [[nodiscard]] bool IsValidLineNumber(
      LineStarts::LineNumber line_number) const noexcept;
  [[nodiscard]] LineStarts::LineNumber LineFromPos(ByteIndex pos) const
      noexcept(!kDocumentContractExceptionsEnabled);

  // Byte-based views.
  DocumentView View() const noexcept;
  DocumentView PartialView(ByteIndex start, ByteCount count) const
      noexcept(!kDocumentContractExceptionsEnabled);

  // Line-based views.
  LogicalLineRange Lines() const noexcept;
  LogicalLineRange LinesFrom(LineNumber first_line) const
      noexcept(!kDocumentContractExceptionsEnabled);

 private:
  GapBuffer buffer_{};
  LineStarts line_starts_{};
};
