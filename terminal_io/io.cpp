#include "terminal_io/io.h"

#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <limits>
#include <system_error>

namespace terminal::io {
namespace {
[[noreturn]] void Fail(int error, const char* operation) {
  throw std::system_error(error, std::generic_category(), operation);
}
}  // namespace

size_t ReadSome(int fd, std::span<char> bytes) {
  if (bytes.empty()) return 0;
  ssize_t count;
  do {
    count = read(
        fd, bytes.data(),
        std::min(bytes.size(),
                 static_cast<size_t>(std::numeric_limits<ssize_t>::max())));
  } while (count < 0 && errno == EINTR);
  if (count < 0) Fail(errno, "read terminal input");
  return static_cast<size_t>(count);
}

bool HasHangup(int fd) {
  if (fd < 0) Fail(EBADF, "poll terminal input");
  pollfd descriptor{.fd = fd, .events = POLLIN, .revents = 0};
  int result;
  do {
    result = poll(&descriptor, 1, 0);
  } while (result < 0 && errno == EINTR);
  if (result < 0) Fail(errno, "poll terminal input");
  if (descriptor.revents & POLLNVAL) Fail(EBADF, "poll terminal input");
  if (descriptor.revents & POLLHUP) return true;
  if (descriptor.revents & POLLERR) Fail(EIO, "poll terminal input");
  return false;
}

bool IsTerminal(int fd) {
  int result;
  do {
    errno = 0;
    result = isatty(fd);
  } while (!result && errno == EINTR);
  if (result) return true;
  if (errno == ENOTTY) return false;
  Fail(errno ? errno : EIO, "isatty terminal input");
}

void WriteAll(int fd, std::string_view bytes) {
  while (!bytes.empty()) {
    const size_t count = std::min(
        bytes.size(), static_cast<size_t>(std::numeric_limits<ssize_t>::max()));
    const ssize_t written = write(fd, bytes.data(), count);
    if (written < 0) {
      if (errno == EINTR) continue;
      Fail(errno, "write terminal output");
    }
    if (written == 0) Fail(EIO, "write terminal output made no progress");
    bytes.remove_prefix(static_cast<size_t>(written));
  }
}

}  // namespace terminal::io
