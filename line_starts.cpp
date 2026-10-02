#include "line_starts.h"

#include <algorithm>

[[nodiscard]] unplugged::dbc::InvariantResult LineStarts::check_invariants()
    const {
  DBC_INVARIANT(std::is_sorted(line_starts_.begin(), line_starts_.end()),
                "line_starts_ must be sorted in ascending order");
  return {};
}

LineStarts::LineStarts(std::vector<ByteIndex> &&line_starts)
    : line_starts_{std::move(line_starts)} {
  DBC_GUARD_CLASS_INVARIANTS();
}

[[nodiscard]] size_t LineStarts::NumLines() const noexcept {
  return line_starts_.size();
}

[[nodiscard]] ByteIndex LineStarts::StartOfLine(LineNumber line_number) const
    noexcept(!kLineStartsContractExceptionsEnabled) {
  DBC_PRE(IsValidLineNumber(line_number),
          "line number must be valid. line_number={}, NumLines()={}",
          line_number, NumLines());
  return line_starts_[line_number];
}

[[nodiscard]] bool LineStarts::IsValidLineNumber(
    LineNumber line_number) const noexcept {
  return line_number < NumLines();
}

[[nodiscard]] LineStarts::LineNumber LineStarts::LineFromPos(
    ByteIndex pos) const noexcept(!kLineStartsContractExceptionsEnabled) {
  DBC_PRE(!line_starts_.empty(),
          "line_starts_ must not be empty. NumLines()={}", NumLines());

  // Find the first line after the one that contains pos.
  auto it = std::upper_bound(line_starts_.begin(), line_starts_.end(), pos);
  return static_cast<LineNumber>(std::distance(line_starts_.begin(), it) - 1);
}

void LineStarts::UpdateOnInsert(ByteIndex pos, ByteSpan bytes) noexcept(
    !kLineStartsContractExceptionsEnabled) {
  DBC_GUARD_CLASS_INVARIANTS();

  const ByteCount count = bytes.size();

  // Update line starts from the line after the one that contains pos.
  auto shift_begin =
      std::upper_bound(line_starts_.begin(), line_starts_.end(), pos);
  for (auto it = shift_begin; it != line_starts_.end(); ++it) {
    *it += count;
  }

  // Line starts are produced in ascending order, so we can reuse the insertion
  // point.
  auto insert_pos = shift_begin;
  for (ByteIndex i = 0; i < count; ++i) {
    if (bytes[i] == '\n') {
      const ByteIndex new_line_start = pos + i + 1;
      insert_pos =
          std::upper_bound(insert_pos, line_starts_.end(), new_line_start);
      insert_pos = line_starts_.insert(insert_pos, new_line_start);
    }
  }
}

void LineStarts::UpdateOnDelete(ByteIndex pos, ByteCount count) noexcept(
    !kLineStartsContractExceptionsEnabled) {
  DBC_GUARD_CLASS_INVARIANTS();

  if (count == 0) {
    return;
  }

  const ByteIndex end_pos = pos + count;

  // Remove line starts in (pos, end_pos]. The start at pos remains; a start at
  // end_pos is removed because the deletion also removes the newline before it.
  auto delete_begin =
      std::upper_bound(line_starts_.begin(), line_starts_.end(), pos);

  // The first line always starts at 0, so deleting from the start must
  // preserve it.
  if (pos == 0 && delete_begin != line_starts_.end() && *delete_begin == 0) {
    ++delete_begin;
  }

  auto delete_end =
      std::upper_bound(line_starts_.begin(), line_starts_.end(), end_pos);

  auto shift_begin = line_starts_.erase(delete_begin, delete_end);

  for (auto it = shift_begin; it != line_starts_.end(); ++it) {
    *it -= count;
  }
}
