#pragma once

#include <gtest/gtest.h>

#include <array>
#include <span>

#include "key_decoder/ports/byte_source/byte_source.h"

namespace unplugged::byte_source_conformance {

inline std::array<uint8_t, 256> AllBytes() {
  std::array<uint8_t, 256> bytes{};
  for (size_t index = 0; index < bytes.size(); ++index) {
    bytes[index] = static_cast<uint8_t>(index);
  }
  return bytes;
}

inline void ExpectOrderedBytes(ByteSource& source,
                               std::span<const uint8_t> bytes) {
  for (const auto byte : bytes) {
    SCOPED_TRACE(static_cast<unsigned>(byte));
    EXPECT_EQ(source.ReadByte(), ByteReadResult{byte});
  }
}

inline void ExpectTimeout(ByteSource& source) {
  EXPECT_EQ(source.ReadByte(), ByteReadResult{ByteReadStatus::Timeout});
}

inline void ExpectEof(ByteSource& source) {
  EXPECT_EQ(source.ReadByte(), ByteReadResult{ByteReadStatus::Eof});
}

}  // namespace unplugged::byte_source_conformance
