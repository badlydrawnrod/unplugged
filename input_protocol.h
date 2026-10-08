#pragma once

#include "terminal.h"

// Legacy stdin/stdout protocol negotiation, kept separate from termios state.
// The probe/debug-output hack will be replaced in the protocol adapter step.
void EnableInputProtocol(terminal::Output& output);
// Checked normal teardown; callers must use best-effort cleanup while
// unwinding.
void DisableInputProtocol(terminal::Output& output);
