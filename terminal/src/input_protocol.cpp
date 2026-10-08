#include "terminal/input_protocol.h"

#include "terminal/output.h"

namespace terminal {

InputProtocol::InputProtocol(Output& output) : output_(output) {
  // A failed write may have delivered the push before reporting an error.
  // Attempt rollback while preserving the acquisition failure.
  output_.PutString("\x1b[>1u");
  try {
    output_.Flush();
    active_ = true;
  } catch (...) {
    try {
      output_.PutString("\x1b[<u");
      output_.Flush();
    } catch (...) {
    }
    throw;
  }
}

InputProtocol::~InputProtocol() noexcept {
  try {
    Restore();
  } catch (...) {
    // Preserve any primary exception and allow raw-mode restoration.
  }
}

void InputProtocol::Restore() {
  if (!active_) return;
  output_.PutString("\x1b[<u");
  output_.Flush();
  active_ = false;
}

}  // namespace terminal
