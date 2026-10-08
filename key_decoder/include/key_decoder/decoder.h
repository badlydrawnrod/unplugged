#pragma once

#include <optional>

#include "key/key.h"
#include "key_decoder/ports/byte_source/byte_source.h"

namespace unplugged {

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
