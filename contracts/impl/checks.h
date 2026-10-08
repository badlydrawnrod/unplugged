#pragma once

#include <concepts>
#include <cstdlib>
#include <exception>
#include <format>
#include <gsl/gsl>

#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "contracts/invariant_result.h"

namespace unplugged::dbc {
namespace detail {
enum class FailureKind {
  Assertion,
  Precondition,
  Postcondition,
};

enum class InvariantPhase {
  OnEntry,
  OnExit,
};

inline constexpr std::string_view failure_name(FailureKind kind) noexcept {
  switch (kind) {
    case FailureKind::Assertion:
      return "ASSERTION FAILED";
    case FailureKind::Precondition:
      return "PRECONDITION FAILED";
    case FailureKind::Postcondition:
      return "POSTCONDITION FAILED";
    default:
      std::unreachable();
  }
}

inline constexpr std::string_view phase_name(InvariantPhase phase) noexcept {
  switch (phase) {
    case InvariantPhase::OnEntry:
      return "on entry";
    case InvariantPhase::OnExit:
      return "on exit";
    default:
      std::unreachable();
  }
}

[[noreturn]] inline void report_failure(
    FailureKind kind, const char *expr,
    std::source_location loc = std::source_location::current()) {
  std::cerr << failure_name(kind) << "!\n"
            << "Expression: " << expr << "\n"
            << "File:       " << loc.file_name() << ":" << loc.line() << "\n"
            << "Function:   " << loc.function_name() << "\n";
  std::abort();
}

[[noreturn]] inline void report_failure(
    FailureKind kind, const char *expr, std::string_view details,
    std::source_location loc = std::source_location::current()) {
  std::cerr << failure_name(kind) << "!\n"
            << "Expression: " << expr << "\n"
            << details << "\n"
            << "File:       " << loc.file_name() << ":" << loc.line() << "\n"
            << "Function:   " << loc.function_name() << "\n";
  std::abort();
}

[[noreturn]] inline void report_invariant_failure(
    const InvariantViolation &violation, InvariantPhase phase,
    std::source_location guard_location) {
  std::cerr << "CLASS INVARIANT FAILED!\n"
            << "Phase:                 " << phase_name(phase) << "\n"
            << "Expression:            " << violation.expression << "\n";

  if (!violation.details.empty()) {
    std::cerr << "Details:               " << violation.details << "\n";
  }

  std::cerr << "Invariant defined at:  "
            << violation.invariant_location.file_name() << ":"
            << violation.invariant_location.line() << "\n"
            << "Invariant function:    "
            << violation.invariant_location.function_name() << "\n"
            << "Guarded scope at:      " << guard_location.file_name() << ":"
            << guard_location.line() << "\n"
            << "Guarded function:      " << guard_location.function_name()
            << "\n";
  std::abort();
}

inline std::string format_details() { return {}; }

template <typename... Args>
inline std::string format_details(std::format_string<Args...> fmt,
                                  Args &&...args) {
  return std::format(fmt, std::forward<Args>(args)...);
}

template <typename CheckFn>
[[nodiscard]] auto make_post_guard(
    CheckFn check, std::source_location loc = std::source_location::current()) {
  return gsl::finally([check, loc]() { check(loc); });
}

[[noreturn]] inline void throw_contract_violation(
    FailureKind kind, const char *expr, std::string_view details = "") {
  std::string msg = std::string(failure_name(kind)) + ": " + expr;
  if (!details.empty()) {
    msg += "\n" + std::string(details);
  }
  throw std::logic_error(msg);
}
}  // namespace detail

template <typename T>
[[nodiscard]] auto make_invariant_guard(
    const T &obj,
    std::source_location caller = std::source_location::current()) {
  static_assert(
      requires(const T &t) {
        { t.check_invariants() } -> std::same_as<InvariantResult>;
      },
      "DBC_GUARD_CLASS_INVARIANTS() requires: "
      "unplugged::dbc::InvariantResult check_invariants() const");

  auto check = [&obj, caller](detail::InvariantPhase phase) {
    if (auto violation = obj.check_invariants(); violation.has_value())
        [[unlikely]]
    {
      detail::report_invariant_failure(*violation, phase, caller);
    }
  };

  check(detail::InvariantPhase::OnEntry);
  int uncaught_on_entry = std::uncaught_exceptions();
  return gsl::finally([check, uncaught_on_entry]() {
    // Only check invariants on exit if no exception is being propagated.
    // This allows precondition violations to throw without triggering
    // invariant checks.
    if (std::uncaught_exceptions() == uncaught_on_entry) {
      check(detail::InvariantPhase::OnExit);
    }
  });
}
}  // namespace unplugged::dbc

// Helper macros to generate unique variable names (e.g., dbc_post_42)
#define DBC_CONCAT_IMPL(a, b) a##b
#define DBC_CONCAT(a, b) DBC_CONCAT_IMPL(a, b)

// ─── Assertion quick reference ─────────────────────────────────────────────
//  DBC_ASSERT(x > 0)               General assertion
//  DBC_ASSERT(x > 0, "x={}", x)    With diagnostic message
//
//  DBC_PRE(x > 0)                  Precondition — checked on function entry
//  DBC_POST(result >= 0)           Postcondition — checked on function exit
//
//  DBC_GUARD_CLASS_INVARIANTS()    Checks invariants on entry and exit
//                                  Requires: unplugged::dbc::InvariantResult
//                                  check_invariants() const Use
//                                  DBC_INVARIANT(expr) inside the detailed
//                                  form
//
// Contract Violation Behavior:
//  - NDEBUG defined (Release):        All contracts are no-ops
//  - NDEBUG undefined (Debug):
//    * WITHOUT CONTRACT_EXCEPTIONS:   Violations abort with detailed
//    diagnostics
//    * WITH CONTRACT_EXCEPTIONS:      Violations throw std::logic_error
//    (testable)
// ───────────────────────────────────────────────────────────────────────────

#if defined(NDEBUG)
#define DBC_ASSERT(expr, ...) ((void)0)
#define DBC_PRE(expr, ...) ((void)0)
#define DBC_POST(expr, ...) ((void)0)
#define DBC_INVARIANT(expr, ...) ((void)0)
#else

#if defined(CONTRACT_EXCEPTIONS)
#define DBC_ASSERT(expr, ...)                                            \
  do {                                                                   \
    if (!(expr)) [[unlikely]] {                                          \
      ::unplugged::dbc::detail::throw_contract_violation(                \
          ::unplugged::dbc::detail::FailureKind::Assertion,              \
          #expr __VA_OPT__(                                              \
              , ::unplugged::dbc::detail::format_details(__VA_ARGS__))); \
    }                                                                    \
  } while (0)

#define DBC_PRE(expr, ...)                                               \
  do {                                                                   \
    if (!(expr)) [[unlikely]] {                                          \
      ::unplugged::dbc::detail::throw_contract_violation(                \
          ::unplugged::dbc::detail::FailureKind::Precondition,           \
          #expr __VA_OPT__(                                              \
              , ::unplugged::dbc::detail::format_details(__VA_ARGS__))); \
    }                                                                    \
  } while (0)

#define DBC_POST(expr, ...)                                                  \
  const auto DBC_CONCAT(dbc_post_, __LINE__) =                               \
      ::unplugged::dbc::detail::make_post_guard([&](std::source_location) {  \
        if (!(expr)) [[unlikely]] {                                          \
          ::unplugged::dbc::detail::throw_contract_violation(                \
              ::unplugged::dbc::detail::FailureKind::Postcondition,          \
              #expr __VA_OPT__(                                              \
                  , ::unplugged::dbc::detail::format_details(__VA_ARGS__))); \
        }                                                                    \
      })

#define DBC_INVARIANT(expr, ...)                                         \
  do {                                                                   \
    if (!(expr)) [[unlikely]] {                                          \
      ::unplugged::dbc::detail::throw_contract_violation(                \
          ::unplugged::dbc::detail::FailureKind::Assertion,              \
          #expr __VA_OPT__(                                              \
              , ::unplugged::dbc::detail::format_details(__VA_ARGS__))); \
    }                                                                    \
  } while (0)

#else /* !defined(CONTRACT_EXCEPTIONS) */

#define DBC_ASSERT(expr, ...)                                            \
  do {                                                                   \
    if (!(expr)) [[unlikely]] {                                          \
      ::unplugged::dbc::detail::report_failure(                          \
          ::unplugged::dbc::detail::FailureKind::Assertion,              \
          #expr __VA_OPT__(                                              \
              , ::unplugged::dbc::detail::format_details(__VA_ARGS__))); \
    }                                                                    \
  } while (0)

#define DBC_PRE(expr, ...)                                               \
  do {                                                                   \
    if (!(expr)) [[unlikely]] {                                          \
      ::unplugged::dbc::detail::report_failure(                          \
          ::unplugged::dbc::detail::FailureKind::Precondition,           \
          #expr __VA_OPT__(                                              \
              , ::unplugged::dbc::detail::format_details(__VA_ARGS__))); \
    }                                                                    \
  } while (0)

#define DBC_POST(expr, ...)                                                    \
  const auto DBC_CONCAT(dbc_post_, __LINE__) =                                 \
      ::unplugged::dbc::detail::make_post_guard(                               \
          [&](std::source_location loc) {                                      \
            if (!(expr)) [[unlikely]] {                                        \
              ::unplugged::dbc::detail::report_failure(                        \
                  ::unplugged::dbc::detail::FailureKind::Postcondition,        \
                  #expr __VA_OPT__(, ::unplugged::dbc::detail::format_details( \
                                         __VA_ARGS__)),                        \
                  loc);                                                        \
            }                                                                  \
          })

#define DBC_INVARIANT(expr, ...)                                        \
  do {                                                                  \
    if (!(expr)) [[unlikely]] {                                         \
      return ::unplugged::dbc::InvariantViolation{                      \
          #expr, ::unplugged::dbc::detail::format_details(__VA_ARGS__), \
          std::source_location::current()};                             \
    }                                                                   \
  } while (0)

#endif /* defined(CONTRACT_EXCEPTIONS) */

#endif

#if defined(NDEBUG)
#define DBC_GUARD_CLASS_INVARIANTS() ((void)0)
#else
#define DBC_GUARD_CLASS_INVARIANTS()            \
  const auto DBC_CONCAT(dbc_guard_, __LINE__) = \
      ::unplugged::dbc::make_invariant_guard(*this)
#endif
