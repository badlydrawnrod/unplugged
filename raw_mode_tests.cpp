#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <stdexcept>
#include <system_error>
#include <type_traits>

#include "raw_mode.h"

namespace {

static_assert(!std::is_default_constructible_v<terminal::RawMode>);
static_assert(!std::is_copy_constructible_v<terminal::RawMode>);
static_assert(!std::is_copy_assignable_v<terminal::RawMode>);
static_assert(!std::is_move_constructible_v<terminal::RawMode>);
static_assert(!std::is_move_assignable_v<terminal::RawMode>);
static_assert(std::is_nothrow_destructible_v<terminal::RawMode>);

// Each test owns an isolated PTY; no process stdin or user terminal is changed.
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

termios Attributes(int fd) {
  termios attributes{};
  if (tcgetattr(fd, &attributes) < 0) {
    throw std::system_error(errno, std::generic_category(), "test tcgetattr");
  }
  return attributes;
}

void SetAttributes(int fd, const termios& attributes) {
  ASSERT_EQ(tcsetattr(fd, TCSANOW, &attributes), 0);
}

void ExpectSame(const termios& actual, const termios& expected) {
  EXPECT_EQ(actual.c_iflag, expected.c_iflag);
  EXPECT_EQ(actual.c_oflag, expected.c_oflag);
  EXPECT_EQ(actual.c_cflag, expected.c_cflag);
  EXPECT_EQ(actual.c_lflag, expected.c_lflag);
  EXPECT_EQ(cfgetispeed(&actual), cfgetispeed(&expected));
  EXPECT_EQ(cfgetospeed(&actual), cfgetospeed(&expected));
  for (size_t i = 0; i < NCCS; ++i) {
    EXPECT_EQ(actual.c_cc[i], expected.c_cc[i]);
  }
}

TEST(RawModeTest, DisablesCanonicalEchoAndSignalProcessingWithTimedReads) {
  PseudoTerminal terminal;
  terminal::RawMode raw_mode(terminal.Slave());
  const auto raw = Attributes(terminal.Slave());
  EXPECT_EQ(raw.c_lflag & ICANON, tcflag_t{0});
  EXPECT_EQ(raw.c_lflag & ECHO, tcflag_t{0});
  EXPECT_EQ(raw.c_lflag & ISIG, tcflag_t{0});
  EXPECT_EQ(raw.c_lflag & IEXTEN, tcflag_t{0});
  EXPECT_EQ(raw.c_iflag & ICRNL, tcflag_t{0});
  EXPECT_EQ(raw.c_iflag & IXON, tcflag_t{0});
  EXPECT_EQ(raw.c_oflag & OPOST, tcflag_t{0});
  EXPECT_EQ(raw.c_cc[VMIN], cc_t{0});
  EXPECT_EQ(raw.c_cc[VTIME], cc_t{1});
}

// Feature: features/raw_mode.feature
// Scenario: Leaving raw mode restores the previous terminal settings
TEST(RawModeTest, ScopeExitRestoresEverySavedTerminalSetting) {
  PseudoTerminal terminal;
  termios custom = Attributes(terminal.Slave());
  custom.c_cc[VMIN] = 7;
  custom.c_cc[VTIME] = 4;
  SetAttributes(terminal.Slave(), custom);
  const termios original = Attributes(terminal.Slave());
  {
    terminal::RawMode raw_mode(terminal.Slave());
    EXPECT_EQ(Attributes(terminal.Slave()).c_lflag & ICANON, tcflag_t{0});
  }
  ExpectSame(Attributes(terminal.Slave()), original);
  EXPECT_GE(fcntl(terminal.Slave(), F_GETFD), 0);
}

// Feature: features/raw_mode.feature
// Scenario: An exception restores the terminal before error handling
TEST(RawModeTest, ExceptionUnwindingRestoresTerminalSettings) {
  PseudoTerminal terminal;
  const termios original = Attributes(terminal.Slave());
  const auto fail_in_raw_mode = [&] {
    terminal::RawMode raw_mode(terminal.Slave());
    throw std::runtime_error("editor failure");
  };
  EXPECT_THROW(fail_in_raw_mode(), std::runtime_error);
  ExpectSame(Attributes(terminal.Slave()), original);
}

// Feature: features/raw_mode.feature
// Scenario: Independent terminal sessions restore their own settings
TEST(RawModeTest, IndependentTerminalsDoNotOverwriteSavedSettings) {
  PseudoTerminal first;
  PseudoTerminal second;
  auto custom = Attributes(second.Slave());
  custom.c_cc[VMIN] = 9;
  SetAttributes(second.Slave(), custom);
  const termios first_original = Attributes(first.Slave());
  const termios second_original = Attributes(second.Slave());
  terminal::RawMode first_mode(first.Slave());
  terminal::RawMode second_mode(second.Slave());
  const termios second_raw = Attributes(second.Slave());
  first_mode.Restore();
  ExpectSame(Attributes(first.Slave()), first_original);
  ExpectSame(Attributes(second.Slave()), second_raw);
  second_mode.Restore();
  ExpectSame(Attributes(second.Slave()), second_original);
}

TEST(RawModeTest, NestedScopesRestoreInLifoOrder) {
  PseudoTerminal terminal;
  const termios original = Attributes(terminal.Slave());
  {
    terminal::RawMode outer(terminal.Slave());
    const termios outer_raw = Attributes(terminal.Slave());
    {
      terminal::RawMode inner(terminal.Slave());
    }
    ExpectSame(Attributes(terminal.Slave()), outer_raw);
  }
  ExpectSame(Attributes(terminal.Slave()), original);
}

TEST(RawModeTest, ExplicitRestoreDisarmsDestructorAndIsIdempotent) {
  PseudoTerminal terminal;
  auto custom = Attributes(terminal.Slave());
  {
    terminal::RawMode raw_mode(terminal.Slave());
    raw_mode.Restore();
    ExpectSame(Attributes(terminal.Slave()), custom);
    custom.c_cc[VMIN] = 3;
    SetAttributes(terminal.Slave(), custom);
    custom = Attributes(terminal.Slave());
    raw_mode.Restore();
    ExpectSame(Attributes(terminal.Slave()), custom);
  }
  ExpectSame(Attributes(terminal.Slave()), custom);
}

TEST(RawModeTest, InvalidDescriptorReportsAcquisitionFailure) {
  try {
    terminal::RawMode raw_mode(-1);
    FAIL() << "Expected an invalid descriptor to fail";
  } catch (const std::system_error& error) {
    EXPECT_EQ(error.code(), std::error_code(EBADF, std::generic_category()));
  }
}

// Feature: features/raw_mode.feature
// Scenario: Raw mode rejects nonterminal input
TEST(RawModeTest, NonterminalDescriptorReportsAcquisitionFailure) {
  const int fd = open("/dev/null", O_RDONLY);
  ASSERT_GE(fd, 0);
  EXPECT_THROW(terminal::RawMode{fd}, std::system_error);
  close(fd);
}

// Feature: features/raw_mode.feature
// Scenario: Restoration failure is reported without throwing during scope exit
TEST(RawModeTest,
     DisconnectedTerminalReportsRestoreFailureWithoutThrowingOnExit) {
  PseudoTerminal terminal;
  terminal::RawMode raw_mode(terminal.Slave());
  terminal.Disconnect();
  EXPECT_THROW(raw_mode.Restore(), std::system_error);
  // Destructor retries best-effort restoration without throwing.
}

}  // namespace
