#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>

using Byte = std::uint8_t;

using ByteIndex = uint32_t;
using ByteCount = uint32_t;

// Leave room for the initial line: even an all-newline document's line count
// and exclusive line-range end must fit in 32 bits. Storage offsets must also
// fit the iterator difference type.
inline constexpr ByteCount kMaxDocumentBytes =
    std::min<std::uintmax_t>(std::numeric_limits<ByteCount>::max(),
                             std::numeric_limits<std::ptrdiff_t>::max()) -
    1;

using ByteSpan = std::span<const Byte>;

inline ByteSpan AsByteSpan(std::string_view text) {
  return {reinterpret_cast<const Byte *>(text.data()), text.size()};
}

inline ByteSpan AsByteSpan(const std::string &text) {
  return AsByteSpan(std::string_view{text});
}

inline ByteSpan AsByteSpan(const char *text) {
  return text == nullptr ? ByteSpan{} : AsByteSpan(std::string_view{text});
}
