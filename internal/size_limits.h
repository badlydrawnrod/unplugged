#pragma once

#include <algorithm>
#include <cstddef>
#include <stdexcept>

#include "types.h"

namespace unplugged::internal {

// Physical storage includes the gap; every physical offset must fit too.
inline constexpr ByteCount kMaxStorageBytes = kMaxDocumentBytes + 1;
static_assert(kMaxStorageBytes <= std::numeric_limits<ByteIndex>::max());

constexpr size_t CheckedAdd(size_t current, size_t added, size_t limit) {
  if (current > limit || added > limit - current) {
    throw std::length_error("size exceeds supported limit");
  }
  return current + added;
}

constexpr ByteCount CheckedByteCount(size_t size, ByteCount limit) {
  return static_cast<ByteCount>(CheckedAdd(0, size, limit));
}

constexpr ByteCount CheckedByteGrowth(ByteCount current, size_t added,
                                      ByteCount limit) {
  return static_cast<ByteCount>(CheckedAdd(current, added, limit));
}

constexpr ByteCount GapGrowth(ByteCount storage_size, ByteCount gap_size,
                              ByteCount needed) {
  constexpr ByteCount kInitialGapSize = 4;
  constexpr ByteCount kGrowDivisor = 8;
  if (needed <= gap_size) {
    return 0;
  }
  const ByteCount required = needed - gap_size;
  // Validate before subtracting to calculate remaining capacity.
  CheckedByteGrowth(storage_size, required, kMaxStorageBytes);
  const ByteCount available = kMaxStorageBytes - storage_size;
  const ByteCount preferred =
      std::max(kInitialGapSize, storage_size / kGrowDivisor);
  return std::max(required, std::min(preferred, available));
}

}  // namespace unplugged::internal
