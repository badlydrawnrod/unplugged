#include <gtest/gtest.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <string>
#include <system_error>
#include <vector>

#include "terminal_io/io.h"

namespace {
// Focused POSIX implementation tests: link wrapping injects syscall outcomes
// without production hooks. Assertions check results and delivered bytes, not
// the number/order of private calls. Other descriptors use the real syscalls.
constexpr int kScriptFd = 100000;
struct Step {
  ssize_t result;
  int error = 0;
  std::string bytes;
};
struct Script {
  std::vector<Step> steps;
  size_t next = 0;
  std::string delivered;
  short poll_events = 0;
};
Script* current = nullptr;
class ScopedScript {
 public:
  explicit ScopedScript(Script& script) { current = &script; }
  ~ScopedScript() { current = nullptr; }
};
Step Next() {
  if (current->next == current->steps.size()) return {-1, EIO, {}};
  return current->steps[current->next++];
}
}  // namespace

extern "C" ssize_t __real_read(int, void*, size_t);
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" int __real_poll(pollfd*, nfds_t, int);
extern "C" int __real_isatty(int);

extern "C" ssize_t __wrap_read(int fd, void* buffer, size_t count) {
  if (fd != kScriptFd || !current) return __real_read(fd, buffer, count);
  const auto step = Next();
  if (step.result < 0) {
    errno = step.error;
    return -1;
  }
  const size_t size = std::min(count, step.bytes.size());
  std::memcpy(buffer, step.bytes.data(), size);
  return static_cast<ssize_t>(size);
}
extern "C" ssize_t __wrap_write(int fd, const void* buffer, size_t count) {
  if (fd != kScriptFd || !current) return __real_write(fd, buffer, count);
  const auto step = Next();
  if (step.result < 0) {
    errno = step.error;
    return -1;
  }
  const size_t size = std::min(count, static_cast<size_t>(step.result));
  current->delivered.append(static_cast<const char*>(buffer), size);
  return static_cast<ssize_t>(size);
}
extern "C" int __wrap_poll(pollfd* descriptors, nfds_t count, int timeout) {
  if (count != 1 || descriptors[0].fd != kScriptFd || !current)
    return __real_poll(descriptors, count, timeout);
  const auto step = Next();
  errno = step.error;
  descriptors[0].revents = current->poll_events;
  return static_cast<int>(step.result);
}
extern "C" int __wrap_isatty(int fd) {
  if (fd != kScriptFd || !current) return __real_isatty(fd);
  const auto step = Next();
  errno = step.error;
  return static_cast<int>(step.result);
}

namespace {
TEST(TerminalIoTest, InterruptedReadEventuallyReturnsItsByte) {
  Script script{{Step{-1, EINTR, {}}, Step{1, 0, "x"}}, 0, {}, 0};
  ScopedScript scope(script);
  std::array<char, 1> buffer{};
  EXPECT_EQ(terminal::io::ReadSome(kScriptFd, buffer), size_t{1});
  EXPECT_EQ(buffer[0], 'x');
}

TEST(TerminalIoTest, ReadErrorRetainsItsErrorCodeAfterInterruption) {
  Script script{{Step{-1, EINTR, {}}, Step{-1, EIO, {}}}, 0, {}, 0};
  ScopedScript scope(script);
  std::array<char, 1> buffer{};
  try {
    terminal::io::ReadSome(kScriptFd, buffer);
    FAIL() << "Expected read failure";
  } catch (const std::system_error& error) {
    EXPECT_EQ(error.code(), std::error_code(EIO, std::generic_category()));
  }
}

TEST(TerminalIoTest, ShortAndInterruptedWritesDeliverExactlyTheOriginalBytes) {
  Script script{
      {Step{2, 0, {}}, Step{-1, EINTR, {}}, Step{1, 0, {}}, Step{2, 0, {}}},
      0,
      {},
      0};
  ScopedScript scope(script);
  terminal::io::WriteAll(kScriptFd, "hello");
  EXPECT_EQ(script.delivered, "hello");
}

TEST(TerminalIoTest, ZeroWriteReportsFailureRatherThanSpinning) {
  Script script{{Step{0, 0, {}}}, 0, {}, 0};
  ScopedScript scope(script);
  try {
    terminal::io::WriteAll(kScriptFd, "hello");
    FAIL() << "Expected no-progress failure";
  } catch (const std::system_error& error) {
    EXPECT_EQ(error.code(), std::error_code(EIO, std::generic_category()));
  }
}

TEST(TerminalIoTest, WriteFailureReportsErrorAfterPartialDelivery) {
  Script script{{Step{2, 0, {}}, Step{-1, ENOSPC, {}}}, 0, {}, 0};
  ScopedScript scope(script);
  try {
    terminal::io::WriteAll(kScriptFd, "hello");
    FAIL() << "Expected write failure";
  } catch (const std::system_error& error) {
    EXPECT_EQ(error.code(), std::error_code(ENOSPC, std::generic_category()));
  }
  EXPECT_EQ(script.delivered, "he");
}

TEST(TerminalIoTest, PollRetriesInterruptionAndDetectsHangup) {
  Script script{{Step{-1, EINTR, {}}, Step{1, 0, {}}}, 0, {}, POLLHUP};
  ScopedScript scope(script);
  EXPECT_TRUE(terminal::io::HasHangup(kScriptFd));
}

TEST(TerminalIoTest, PollReportsInvalidDescriptorAndEndpointError) {
  for (auto events : {POLLNVAL, POLLERR}) {
    Script script{{Step{1, 0, {}}}, 0, {}, static_cast<short>(events)};
    ScopedScript scope(script);
    EXPECT_THROW(terminal::io::HasHangup(kScriptFd), std::system_error);
  }
}

TEST(TerminalIoTest, PollSyscallFailureIsReported) {
  Script script{{Step{-1, EIO, {}}}, 0, {}, 0};
  ScopedScript scope(script);
  EXPECT_THROW(terminal::io::HasHangup(kScriptFd), std::system_error);
}

TEST(TerminalIoTest, IsattyRetriesInterruptionAndRecognizesTerminal) {
  Script script{{Step{0, EINTR, {}}, Step{1, 0, {}}}, 0, {}, 0};
  ScopedScript scope(script);
  EXPECT_TRUE(terminal::io::IsTerminal(kScriptFd));
}

TEST(TerminalIoTest, NonterminalIsDistinctFromIsattyFailure) {
  {
    Script script{{Step{0, ENOTTY, {}}}, 0, {}, 0};
    ScopedScript scope(script);
    EXPECT_FALSE(terminal::io::IsTerminal(kScriptFd));
  }
  {
    Script script{{Step{0, EBADF, {}}}, 0, {}, 0};
    ScopedScript scope(script);
    EXPECT_THROW(terminal::io::IsTerminal(kScriptFd), std::system_error);
  }
}
}  // namespace
