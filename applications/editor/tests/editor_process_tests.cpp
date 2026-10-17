#include <fcntl.h>
#include <gtest/gtest.h>
#include <poll.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <string>
#include <string_view>
#include <system_error>

#include "test_support/posix_endpoints.h"

namespace {

using namespace std::chrono_literals;
using unplugged::test_support::PseudoTerminal;

constexpr auto kProcessTimeout = 3s;
std::string editor_binary;

class TemporaryFile {
 public:
  TemporaryFile() {
    path_ = testing::TempDir() + "unplugged-editor-XXXXXX";
    const int fd = mkstemp(path_.data());
    if (fd < 0) {
      throw std::system_error(errno, std::generic_category(), "create file");
    }

    constexpr char content[] = "editor-process-marker";
    const ssize_t written = write(fd, content, sizeof(content) - 1);
    close(fd);
    if (written != static_cast<ssize_t>(sizeof(content) - 1)) {
      unlink(path_.c_str());
      throw std::runtime_error("could not write the temporary editor file");
    }
  }

  ~TemporaryFile() { unlink(path_.c_str()); }
  TemporaryFile(const TemporaryFile&) = delete;
  TemporaryFile& operator=(const TemporaryFile&) = delete;

  const std::string& Path() const { return path_; }

 private:
  std::string path_;
};

class ChildProcess {
 public:
  explicit ChildProcess(pid_t pid) : pid_(pid) {}
  ~ChildProcess() {
    if (pid_ > 0) {
      kill(pid_, SIGKILL);
      while (waitpid(pid_, nullptr, 0) < 0 && errno == EINTR) {
      }
    }
  }
  ChildProcess(const ChildProcess&) = delete;
  ChildProcess& operator=(const ChildProcess&) = delete;

  int Wait() {
    int status = 0;
    while (waitpid(pid_, &status, 0) < 0) {
      if (errno == EINTR) continue;
      throw std::system_error(errno, std::generic_category(),
                              "wait for editor");
    }
    pid_ = -1;
    return status;
  }

 private:
  pid_t pid_;
};

class FileDescriptor {
 public:
  explicit FileDescriptor(int fd) : fd_(fd) {}
  ~FileDescriptor() {
    if (fd_ >= 0) close(fd_);
  }
  FileDescriptor(const FileDescriptor&) = delete;
  FileDescriptor& operator=(const FileDescriptor&) = delete;
  int Get() const { return fd_; }

 private:
  int fd_;
};

termios Attributes(int fd) {
  termios attributes{};
  if (tcgetattr(fd, &attributes) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "read terminal settings");
  }
  return attributes;
}

bool SameAttributes(const termios& left, const termios& right) {
  if (left.c_iflag != right.c_iflag || left.c_oflag != right.c_oflag ||
      left.c_cflag != right.c_cflag || left.c_lflag != right.c_lflag ||
      cfgetispeed(&left) != cfgetispeed(&right) ||
      cfgetospeed(&left) != cfgetospeed(&right)) {
    return false;
  }
  for (size_t i = 0; i < NCCS; ++i) {
    if (left.c_cc[i] != right.c_cc[i]) return false;
  }
  return true;
}

bool ReadUntil(int fd, std::string_view marker, std::string& output,
               std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (output.find(marker) == std::string::npos) {
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
    if (remaining <= 0ms) return false;

    pollfd event{fd, POLLIN, 0};
    const int result = poll(&event, 1, static_cast<int>(remaining.count()));
    if (result < 0) {
      if (errno == EINTR) continue;
      throw std::system_error(errno, std::generic_category(),
                              "poll terminal output");
    }
    if (result == 0) return false;

    std::array<char, 512> bytes{};
    const ssize_t count = read(fd, bytes.data(), bytes.size());
    if (count < 0) {
      if (errno == EINTR || errno == EAGAIN) continue;
      throw std::system_error(errno, std::generic_category(),
                              "read terminal output");
    }
    if (count == 0) return false;
    output.append(bytes.data(), static_cast<size_t>(count));
  }
  return true;
}

// Feature: applications/editor/features/application.feature
// Scenario: Opening a file and exiting restores the terminal
TEST(EditorApplicationTest, OpensFileAndRestoresTerminalOnAltEscape) {
  TemporaryFile file;
  PseudoTerminal terminal;
  const termios original = Attributes(terminal.Slave());

  const pid_t pid = fork();
  ASSERT_GE(pid, 0);
  if (pid == 0) {
    if (dup2(terminal.Slave(), STDIN_FILENO) < 0 ||
        dup2(terminal.Slave(), STDOUT_FILENO) < 0 ||
        dup2(terminal.Slave(), STDERR_FILENO) < 0) {
      _exit(126);
    }
    close(terminal.Master());
    close(terminal.Slave());
    execl(editor_binary.c_str(), editor_binary.c_str(), file.Path().c_str(),
          static_cast<char*>(nullptr));
    _exit(127);
  }
  ChildProcess child(pid);

  const int pidfd = static_cast<int>(syscall(SYS_pidfd_open, pid, 0));
  ASSERT_GE(pidfd, 0) << "pidfd_open failed: " << errno;
  FileDescriptor process_event(pidfd);

  std::string screen;
  ASSERT_TRUE(ReadUntil(terminal.Master(), "editor-process-marker", screen,
                        kProcessTimeout))
      << "editor did not display the input file before the deadline";

  const termios active = Attributes(terminal.Slave());
  EXPECT_EQ(active.c_lflag & (ICANON | ECHO), tcflag_t{0});

  constexpr char alt_escape[] = {'\x1b', '\x1b'};
  ASSERT_EQ(write(terminal.Master(), alt_escape, sizeof(alt_escape)),
            static_cast<ssize_t>(sizeof(alt_escape)));

  pollfd exited{process_event.Get(), POLLIN, 0};
  const auto deadline = std::chrono::steady_clock::now() + kProcessTimeout;
  int result = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
    result = poll(&exited, 1, static_cast<int>(remaining.count()));
    if (result < 0 && errno == EINTR) continue;
    break;
  }
  ASSERT_EQ(result, 1) << "editor did not exit before the deadline";
  ASSERT_NE(exited.revents & POLLIN, 0);

  const int status = child.Wait();
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);
  EXPECT_TRUE(SameAttributes(Attributes(terminal.Slave()), original));
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc != 2) return 2;
  editor_binary = std::string(std::getenv("TEST_SRCDIR")) + "/" +
                  std::getenv("TEST_WORKSPACE") + "/" + argv[1];
  argc = 1;
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
