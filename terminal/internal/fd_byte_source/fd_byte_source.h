#pragma once

#include "key_decoder/ports/byte_source/byte_source.h"

namespace terminal {

// Borrows fd; its owner controls descriptor and terminal lifetimes. Reads one
// byte at a time. A zero read is Timeout for a terminal without hangup,
// otherwise Eof. Syscall failures throw std::system_error; Error is never
// returned.
class FdByteSource final : public unplugged::ByteSource {
 public:
  explicit FdByteSource(int fd) : fd_(fd) {}

  unplugged::ByteReadResult ReadByte() override;
  bool ReachedEnd() const { return eof_; }

 private:
  int fd_;
  bool eof_ = false;
};

}  // namespace terminal
