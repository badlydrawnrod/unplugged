#include "input_protocol.h"

#include <unistd.h>

#include <format>
#include <iostream>

void EnableInputProtocol() {
  // Probe for kitty input protocol by querying the current values of the
  // flags.
  std::cout << "\x1b[?u";
  std::flush(std::cout);

  // Check that we got back CSI ? flags u
  // TODO: Remove this hack!
  char data[6] = {'\0', '\0', '\0', '\0', '\0', '\0'};
  auto n = read(STDIN_FILENO, &data, sizeof(data));
  std::cout << std::format("n = {}, {:02x}{:02x}{:02x}{:02x}{:02x}{:02x}\r\n",
                           n, data[0], data[1], data[2], data[3], data[4],
                           data[5]);
  if (n >= 5 && data[0] == '\x1b' && data[1] == '[' && data[2] == '?' &&
      data[n - 1] == 'u') {
    std::cout << "\r\nProbably kitty\r\n";
    // Progressive enhancement: disambiguate escape codes. That's all we're
    // going to support.
    std::cout << "\x1b[>1u";
  } else {
    std::cout << "\r\nNo kitty\r\n";
  }
  std::flush(std::cout);
}

void DisableInputProtocol() noexcept {
  try {
    std::cout << "\x1b[<u";
    std::flush(std::cout);
  } catch (...) {
    // Teardown must allow the raw-mode owner to restore terminal attributes.
  }
}
