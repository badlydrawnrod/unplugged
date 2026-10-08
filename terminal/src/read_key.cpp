#include "terminal/read_key.h"

#include <unistd.h>

#include <span>

#include "key_decoder/decoder.h"
#include "terminal_io/io.h"

namespace {

class FdByteSource final : public unplugged::ByteSource {
 public:
  explicit FdByteSource(int fd) : fd_(fd) {}

  unplugged::ByteReadResult ReadByte() override {
    char byte = 0;
    if (terminal::io::ReadSome(fd_, std::span(&byte, 1)) == 1) {
      return static_cast<uint8_t>(byte);
    }
    eof_ = terminal::io::HasHangup(fd_) || !terminal::io::IsTerminal(fd_);
    return eof_ ? unplugged::ByteReadStatus::Eof
                : unplugged::ByteReadStatus::Timeout;
  }

  bool ReachedEnd() const { return eof_; }

 private:
  int fd_;
  bool eof_ = false;
};

}  // namespace

KeyReadResult ReadKey(int fd) {
  FdByteSource source(fd);
  if (auto key = unplugged::DecodeKey(source)) return *key;
  return source.ReachedEnd() ? KeyReadStatus::Eof : KeyReadStatus::NoKey;
}

KeyReadResult ReadKey() { return ReadKey(STDIN_FILENO); }
