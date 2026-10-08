#pragma once

#include <limits>
#include <vector>

#include "contracts/invariant_result.h"
#include "document/detail/gap_buffer.h"
#include "document/detail/line_starts.h"
#include "document/line_number.h"
#include "document/logical_line_range.h"
#include "types.h"

#if defined(CONTRACT_EXCEPTIONS)
inline constexpr bool kDocumentContractExceptionsEnabled = true;
#else
inline constexpr bool kDocumentContractExceptionsEnabled = false;
#endif

class DocumentView;

// Byte edits and document views over privately owned inline storage.
class Document {
 public:
  using LineNumber = unplugged::document::LineNumber;
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
  [[nodiscard]] ByteIndex StartOfLine(LineNumber line_number) const
      noexcept(!kDocumentContractExceptionsEnabled);
  [[nodiscard]] bool IsValidLineNumber(LineNumber line_number) const noexcept;
  [[nodiscard]] LineNumber LineFromPos(ByteIndex pos) const
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
  unplugged::document_detail::GapBuffer buffer_{};
  unplugged::document_detail::LineStarts line_starts_{};
};
