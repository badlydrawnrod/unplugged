#include "terminal/read_key.h"

#include <unistd.h>

#include "key_decoder/decoder.h"
#include "key_decoder/ports/byte_source/byte_source.h"
#include "terminal/internal/fd_byte_source/fd_byte_source.h"

KeyReadResult ReadKey(int fd) {
  terminal::FdByteSource source(fd);
  if (auto key = unplugged::DecodeKey(source)) return *key;
  return source.ReachedEnd() ? KeyReadStatus::Eof : KeyReadStatus::NoKey;
}

KeyReadResult ReadKey() { return ReadKey(STDIN_FILENO); }
