#pragma once

namespace terminal {
class Output;

// Requests kitty escape-code disambiguation using the protocol's push/pop
// quickstart. No support query or input read is needed: legacy input decoding
// remains available on terminals that ignore these commands.
// https://sw.kovidgoyal.net/kitty/keyboard-protocol/#quickstart
// Borrows an active Output, which must outlive this scope. Nested sessions must
// restore in LIFO order. This owns protocol state, not terminal attributes.
class InputProtocol {
 public:
  explicit InputProtocol(Output& output);
  ~InputProtocol() noexcept;
  InputProtocol(const InputProtocol&) = delete;
  InputProtocol& operator=(const InputProtocol&) = delete;
  InputProtocol(InputProtocol&&) = delete;
  InputProtocol& operator=(InputProtocol&&) = delete;

  // Checked, idempotent teardown. Failure retains best-effort destructor
  // cleanup.
  void Restore();

 private:
  Output& output_;
  bool active_ = false;
};
}  // namespace terminal
