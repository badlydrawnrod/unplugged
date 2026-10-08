#pragma once

#include <optional>
#include <source_location>
#include <string>

namespace unplugged::dbc {
struct InvariantViolation {
  const char *expression;
  std::string details;
  std::source_location invariant_location;
};

using InvariantResult = std::optional<InvariantViolation>;

}  // namespace unplugged::dbc
