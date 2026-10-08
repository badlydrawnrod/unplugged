#pragma once

#include <cstdint>
#include <vector>

#include "contracts/invariant_result.h"
#include "types.h"

// Unsupported concrete representation; use the storage subcomponent API.
namespace unplugged::document_detail {

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

  // Size-limit and allocation errors propagate without changing the index.
  void UpdateOnInsert(ByteIndex pos, ByteSpan bytes);

  void UpdateOnDelete(ByteIndex pos, ByteCount count);

  [[nodiscard]] unplugged::dbc::InvariantResult CheckInvariants() const;

 private:
  std::vector<ByteIndex> line_starts_{};
};

}  // namespace unplugged::document_detail
