#pragma once

#include <cstdint>
#include <variant>

namespace unplugged {

// Non-byte outcomes are distinct from every possible byte, including NUL.
// Timeout means no byte is available for this acquisition; later calls may
// supply bytes. Eof means input has ended. Error represents an acquisition
// failure when an implementation reports failures as values rather than throws.
// Status outcomes do not consume a byte. No timing policy is imposed here.
enum class ByteReadStatus : uint8_t { Timeout, Eof, Error };
using ByteReadResult = std::variant<uint8_t, ByteReadStatus>;

// Each successful acquisition consumes exactly one byte in source order,
// preserving all eight bits. Acquisition supplies bytes and interruptions;
// decoding performs no I/O and does not choose timeouts. Implementations may
// read a descriptor or a deterministic stream. Failure reporting is adapter
// specific: the terminal FD adapter throws std::system_error for syscall
// failures, rather than returning Error. Exceptions propagate through decoding.
class ByteSource {
 public:
  virtual ~ByteSource() = default;
  virtual ByteReadResult ReadByte() = 0;
};

}  // namespace unplugged
