#include "internal/test_support/allocation_failure.h"

#include <cstdlib>
#include <limits>
#include <new>

namespace {
constexpr size_t kAllocationFailuresDisabled =
    std::numeric_limits<size_t>::max();
thread_local size_t allocation_countdown = kAllocationFailuresDisabled;
}  // namespace

namespace unplugged::testing {
FailAllocationAfter::FailAllocationAfter(
    size_t successful_allocations) noexcept {
  allocation_countdown = successful_allocations;
}

FailAllocationAfter::~FailAllocationAfter() {
  allocation_countdown = kAllocationFailuresDisabled;
}
}  // namespace unplugged::testing

// Link only into the isolated allocation-failure test executable. Keep these
// definitions separate from clients so compiler inlining does not confuse
// new/delete diagnostics with the malloc/free implementation underneath them.
void* operator new(size_t size) {
  if (allocation_countdown != kAllocationFailuresDisabled) {
    if (allocation_countdown == 0) {
      throw std::bad_alloc{};
    }
    --allocation_countdown;
  }
  if (void* memory = std::malloc(size == 0 ? 1 : size)) {
    return memory;
  }
  throw std::bad_alloc{};
}

void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, size_t) noexcept { std::free(memory); }
