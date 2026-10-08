#include "raw_mode.h"

#include <cerrno>
#include <system_error>

namespace terminal {
namespace {

int GetAttributes(int fd, termios& attributes) {
  int result;
  do {
    result = tcgetattr(fd, &attributes);
  } while (result < 0 && errno == EINTR);
  return result;
}

int SetAttributes(int fd, const termios& attributes) {
  int result;
  do {
    result = tcsetattr(fd, TCSAFLUSH, &attributes);
  } while (result < 0 && errno == EINTR);
  return result;
}

}  // namespace

RawMode::RawMode(int fd) : fd_(fd) {
  if (GetAttributes(fd_, saved_) < 0) {
    throw std::system_error(errno, std::generic_category(), "tcgetattr");
  }
  termios raw = saved_;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~OPOST;
  raw.c_cflag |= CS8;
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  if (SetAttributes(fd_, raw) < 0) {
    const int error = errno;
    // Construction cannot publish an owner on failure. Attempt rollback before
    // propagating the original error, even if restoring also fails.
    SetAttributes(fd_, saved_);
    throw std::system_error(error, std::generic_category(), "tcsetattr(raw)");
  }
  active_ = true;
}

RawMode::~RawMode() noexcept {
  if (active_) SetAttributes(fd_, saved_);
}

void RawMode::Restore() {
  if (!active_) return;
  if (SetAttributes(fd_, saved_) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "tcsetattr(restore)");
  }
  active_ = false;
}

}  // namespace terminal
