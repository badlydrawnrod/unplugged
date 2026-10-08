#include <gtest/gtest.h>
#include <termios.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <system_error>

#include "internal/test_support/posix_endpoints.h"
#include "read_key.h"

namespace {
using unplugged::test_support::Pipe;
using unplugged::test_support::PseudoTerminal;

// Feature: features/terminal_input.feature
// Scenario: Finite input ends after its last decoded key
TEST(ReadKeyTest, FiniteInputReportsKeysThenEof) {
  Pipe pipe;
  ASSERT_EQ(write(pipe.Writer(), "x\x1b[A", 4), ssize_t{4});
  pipe.CloseWriter();
  const auto text = ReadKey(pipe.Reader());
  ASSERT_TRUE(std::holds_alternative<Key>(text));
  EXPECT_EQ(std::get<Key>(text).Text(), "x");
  const auto arrow = ReadKey(pipe.Reader());
  ASSERT_TRUE(std::holds_alternative<Key>(arrow));
  EXPECT_TRUE(std::get<Key>(arrow).Is(NavigationKey::Up));
  EXPECT_EQ(std::get<KeyReadStatus>(ReadKey(pipe.Reader())),
            KeyReadStatus::Eof);
}

// Feature: features/terminal_input.feature
// Scenario: EOF during a sequence remains EOF
TEST(ReadKeyTest, IncompleteEscapeSequenceReportsEof) {
  Pipe pipe;
  ASSERT_EQ(write(pipe.Writer(), "\x1b[", 2), ssize_t{2});
  pipe.CloseWriter();
  EXPECT_EQ(std::get<KeyReadStatus>(ReadKey(pipe.Reader())),
            KeyReadStatus::Eof);
}

TEST(ReadKeyTest, UnsupportedSequenceIsDistinctFromEof) {
  Pipe pipe;
  ASSERT_EQ(write(pipe.Writer(), "\x1b[99~x", 6), ssize_t{6});
  pipe.CloseWriter();
  EXPECT_EQ(std::get<KeyReadStatus>(ReadKey(pipe.Reader())),
            KeyReadStatus::NoKey);
  EXPECT_EQ(std::get<Key>(ReadKey(pipe.Reader())).Text(), "x");
  EXPECT_EQ(std::get<KeyReadStatus>(ReadKey(pipe.Reader())),
            KeyReadStatus::Eof);
}

// Feature: features/terminal_input.feature
// Scenario: Acquisition failures report an error
TEST(ReadKeyTest, InvalidInputDescriptorReportsSystemError) {
  try {
    ReadKey(-1);
    FAIL() << "Expected read failure";
  } catch (const std::system_error& error) {
    EXPECT_EQ(error.code(), std::error_code(EBADF, std::generic_category()));
  }
}

TEST(ReadKeyTest, TerminalZeroReadIsNoKeyWithoutSleeping) {
  PseudoTerminal terminal;
  termios attributes{};
  ASSERT_EQ(tcgetattr(terminal.Slave(), &attributes), 0);
  attributes.c_lflag &= ~ICANON;
  attributes.c_cc[VMIN] = 0;
  attributes.c_cc[VTIME] = 0;
  ASSERT_EQ(tcsetattr(terminal.Slave(), TCSANOW, &attributes), 0);
  EXPECT_EQ(std::get<KeyReadStatus>(ReadKey(terminal.Slave())),
            KeyReadStatus::NoKey);
}

// Feature: features/terminal_input.feature
// Scenario: A terminal hangup ends input
TEST(ReadKeyTest, TerminalHangupReportsEof) {
  PseudoTerminal terminal;
  terminal.Disconnect();
  EXPECT_EQ(std::get<KeyReadStatus>(ReadKey(terminal.Slave())),
            KeyReadStatus::Eof);
}
}  // namespace
