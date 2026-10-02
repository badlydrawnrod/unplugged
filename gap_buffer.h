#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "dsl.h"
#include "types.h"

#if defined(CONTRACT_EXCEPTIONS)
inline constexpr bool kGapBufferContractExceptionsEnabled = true;
#else
inline constexpr bool kGapBufferContractExceptionsEnabled = false;
#endif

// Simple byte-oriented gap buffer.
class GapBuffer {
 public:
  GapBuffer() = default;

  GapBuffer(std::vector<Byte> &&buffer);

  // Pre: pos < Len().
  [[nodiscard]] Byte At(ByteIndex pos) const
      noexcept(!kGapBufferContractExceptionsEnabled);

  // Pre: pos <= Len().
  void Insert(ByteIndex pos, const ByteSpan bytes);

  // Pre: pos <= Len() && count <= Len() - pos.
  void Delete(ByteIndex pos, ByteCount count = 1) noexcept(
      !kGapBufferContractExceptionsEnabled);

  // Pre: start <= Len().
  [[nodiscard]] ByteIndex AppendRange(ByteIndex start, ByteCount count,
                                      std::vector<Byte> &out) const;

  [[nodiscard]] ByteCount Len() const noexcept;

  [[nodiscard]] unplugged::dbc::InvariantResult check_invariants() const;

  bool IsValid(ByteIndex pos) const noexcept;

 private:
  static constexpr ByteCount InitialGapSize = 4;
  static constexpr ByteCount GrowDivisor = 8;

  void GrowGap(ByteCount needed);
  void MoveGapTo(ByteIndex pos) noexcept(!kGapBufferContractExceptionsEnabled);
  [[nodiscard]] ByteCount GapSize() const noexcept;

  std::vector<Byte> data_{};
  ByteIndex left_{};
  ByteIndex right_{};
};
