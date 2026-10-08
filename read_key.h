#pragma once

#include <optional>

#include "key.h"

// Acquires bytes from stdin and delegates to the key decoder. Returns no key
// for an initial timeout, EOF/error, or an unsupported/incomplete sequence.
std::optional<Key> ReadKey();
