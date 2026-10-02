#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

using Byte = std::uint8_t;

using ByteIndex = uint32_t;
using ByteCount = uint32_t;

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
