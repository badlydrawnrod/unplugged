#pragma once

#include <cstdint>
#include <vector>

#include "dsl.h"
#include "types.h"

#if defined(CONTRACT_EXCEPTIONS)
inline constexpr bool kLineStartsContractExceptionsEnabled = true;
#else
inline constexpr bool kLineStartsContractExceptionsEnabled = false;
#endif

class LineStarts {
 public:
  using LineNumber = uint32_t;

  LineStarts() = default;

  LineStarts(std::vector<ByteIndex> &&line_starts);

  [[nodiscard]] size_t NumLines() const noexcept;

  [[nodiscard]] ByteIndex StartOfLine(LineNumber line_number) const
      noexcept(!kLineStartsContractExceptionsEnabled);

  [[nodiscard]] bool IsValidLineNumber(LineNumber line_number) const noexcept;

  [[nodiscard]] LineNumber LineFromPos(ByteIndex pos) const
      noexcept(!kLineStartsContractExceptionsEnabled);

  void UpdateOnInsert(ByteIndex pos, ByteSpan bytes) noexcept(
      !kLineStartsContractExceptionsEnabled);

  void UpdateOnDelete(ByteIndex pos, ByteCount count) noexcept(
      !kLineStartsContractExceptionsEnabled);

  [[nodiscard]] unplugged::dbc::InvariantResult check_invariants() const;

 private:
  std::vector<ByteIndex> line_starts_{};
};
