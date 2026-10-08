#pragma once

// Legacy stdin/stdout protocol negotiation, kept separate from termios state.
// The probe/debug-output hack will be replaced in the protocol adapter step.
void EnableInputProtocol();
// Best-effort teardown is safe during exception unwinding.
void DisableInputProtocol() noexcept;
