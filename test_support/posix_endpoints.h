#pragma once

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <system_error>

namespace unplugged::test_support {

class Pipe {
 public:
  Pipe() {
    if (pipe(fds_) < 0) {
      throw std::system_error(errno, std::generic_category(), "create pipe");
    }
  }
  ~Pipe() {
    CloseReader();
    CloseWriter();
  }
  Pipe(const Pipe&) = delete;
  Pipe& operator=(const Pipe&) = delete;
  int Reader() const { return fds_[0]; }
  int Writer() const { return fds_[1]; }
  void CloseReader() {
    if (fds_[0] >= 0) close(fds_[0]);
    fds_[0] = -1;
  }
  void CloseWriter() {
    if (fds_[1] >= 0) close(fds_[1]);
    fds_[1] = -1;
  }

 private:
  int fds_[2] = {-1, -1};
};

// An isolated PTY: never changes process stdin or the user's terminal.
class PseudoTerminal {
 public:
  PseudoTerminal() {
    master_ = posix_openpt(O_RDWR | O_NOCTTY);
    if (master_ < 0) Fail();
    if (grantpt(master_) < 0 || unlockpt(master_) < 0) Fail();
    const char* name = ptsname(master_);
    if (!name) Fail();
    slave_ = open(name, O_RDWR | O_NOCTTY);
    if (slave_ < 0) Fail();
  }
  ~PseudoTerminal() {
    if (slave_ >= 0) close(slave_);
    if (master_ >= 0) close(master_);
  }
  PseudoTerminal(const PseudoTerminal&) = delete;
  PseudoTerminal& operator=(const PseudoTerminal&) = delete;
  int Slave() const { return slave_; }
  int Master() const { return master_; }
  void Disconnect() {
    close(master_);
    master_ = -1;
  }

 private:
  [[noreturn]] void Fail() {
    const int error = errno;
    if (master_ >= 0) close(master_);
    throw std::system_error(error, std::generic_category(), "create PTY");
  }
  int master_ = -1;
  int slave_ = -1;
};

}  // namespace unplugged::test_support
