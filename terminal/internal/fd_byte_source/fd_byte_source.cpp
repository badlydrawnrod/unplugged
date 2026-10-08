#include "terminal/internal/fd_byte_source/fd_byte_source.h"

#include <span>

#include "terminal_io/io.h"

namespace terminal {

unplugged::ByteReadResult FdByteSource::ReadByte() {
  char byte = 0;
  if (io::ReadSome(fd_, std::span(&byte, 1)) == 1) {
    return static_cast<uint8_t>(byte);
  }
  eof_ = io::HasHangup(fd_) || !io::IsTerminal(fd_);
  return eof_ ? unplugged::ByteReadStatus::Eof
              : unplugged::ByteReadStatus::Timeout;
}

}  // namespace terminal
