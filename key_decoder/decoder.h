#pragma once

#include <cstdint>
#include <optional>
#include <variant>

#include "key.h"

namespace unplugged {

// Non-byte outcomes are distinct from every possible byte, including NUL.
enum class ByteReadStatus : uint8_t { Timeout, Eof, Error };
using ByteReadResult = std::variant<uint8_t, ByteReadStatus>;

// Acquisition supplies bytes and interruptions; decoding performs no I/O and
// does not choose timeouts. Implementations may read stdin or a finite stream.
class ByteSource {
 public:
  virtual ~ByteSource() = default;
  virtual ByteReadResult ReadByte() = 0;
};

// Attempts one key, consuming only its input bytes. Returns no key for an
// initial interruption, unsupported input, or an interrupted sequence. Consumed
// prefixes are discarded, with no parser state retained between calls.
// Escape followed by Timeout is bare Escape; Escape followed by EOF or Error
// produces no key. Modifiers and legacy/kitty mappings retain existing
// behavior.
// Current limitations: UTF-8 scalar validity is not fully checked; legacy Alt
// fallback accepts only ASCII and two/three-byte UTF-8; event suffixes are
// ignored; the uint8_t modifier parser cannot represent protocol field 256.
// Fix these as explicit behavior changes with API tests and scenarios.
std::optional<Key> DecodeKey(ByteSource& source);

}  // namespace unplugged
