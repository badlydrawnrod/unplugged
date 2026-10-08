#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <random>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "document/internal/gap_buffer/gap_buffer.h"

namespace {
std::string Snapshot(const GapBuffer &buffer) {
  std::vector<Byte> out;
  const ByteCount copied = buffer.AppendRange(0, buffer.Len(), out);
  if (copied != buffer.Len()) {
    ADD_FAILURE() << "AppendRange copied " << copied << " bytes, expected "
                  << buffer.Len();
    return {};
  }
  return std::string(out.begin(), out.end());
}

struct ReferenceBuffer {
  std::vector<Byte> data{};

  std::optional<Byte> At(ByteIndex pos) const {
    if (pos < data.size()) {
      return data[pos];
    }
    return {};
  }

  ByteCount Len() const { return data.size(); }

  void Insert(ByteIndex pos, Byte c) {
    if (pos > data.size()) {
      pos = data.size();
    }
    data.insert(data.begin() + static_cast<std::ptrdiff_t>(pos), c);
  }

  void Insert(ByteIndex pos, const ByteSpan sv) {
    if (pos > data.size()) {
      pos = data.size();
    }
    data.insert(data.begin() + static_cast<std::ptrdiff_t>(pos), sv.begin(),
                sv.end());
  }

  void Delete(ByteIndex pos, ByteCount count = 1) {
    if (pos > data.size() || count > data.size() - pos) {
      throw std::out_of_range("delete out of range");
    }
    if (count == 0) {
      return;
    }
    data.erase(data.begin() + static_cast<std::ptrdiff_t>(pos),
               data.begin() + static_cast<std::ptrdiff_t>(pos + count));
  }

  ByteCount CopyRange(ByteIndex start, ByteCount count,
                      std::vector<Byte> &out) const {
    if (start >= data.size() || count == 0) {
      return 0;
    }

    const ByteCount copied = std::min(count, ByteCount(data.size()) - start);
    out.insert(out.end(), data.begin() + static_cast<std::ptrdiff_t>(start),
               data.begin() + static_cast<std::ptrdiff_t>(start + copied));
    return copied;
  }
};

std::string Snapshot(const ReferenceBuffer &buffer) {
  return std::string(buffer.data.begin(), buffer.data.end());
}

enum class OpType {
  InsertString,
  Delete,
  CopyRange,
};

struct Op {
  OpType type{};
  Byte ch{};
  ByteIndex a{};
  ByteCount b{};
  std::string str{};
};

[[maybe_unused]] std::string DescribeOp(const Op &op) {
  switch (op.type) {
    case OpType::InsertString:
      return "InsertString(" + std::to_string(op.a) + ", \"" + op.str + "\")";
    case OpType::Delete:
      return "Delete(" + std::to_string(op.a) + ", " + std::to_string(op.b) +
             ")";
    case OpType::CopyRange:
      return "CopyRange(" + std::to_string(op.a) + ", " + std::to_string(op.b) +
             ")";
  }
  return "unknown";
}

[[maybe_unused]] uint64_t SeedFromEnvOr(uint64_t fallback) {
  const char *env = std::getenv("GAP_BUFFER_TEST_SEED");
  if (env == nullptr) {
    return fallback;
  }

  char *end = nullptr;
  const unsigned long long value = std::strtoull(env, &end, 10);
  if (end != env && *end == '\0') {
    return static_cast<uint64_t>(value);
  }

  return fallback;
}

[[maybe_unused]] Op GenerateRandomOp(std::mt19937_64 &rng, ByteIndex max_value) {
  std::uniform_int_distribution<int> type_dist(0, 2);
  const OpType type = static_cast<OpType>(type_dist(rng));

  std::uniform_int_distribution<ByteIndex> pos_dist(0, max_value);
  Op op{.type = type, .a = pos_dist(rng)};
  switch (type) {
    case OpType::InsertString: {
      std::uniform_int_distribution<ByteCount> len_dist(0, 32);
      std::uniform_int_distribution<int> ch_dist(0, 255);
      const ByteCount len = len_dist(rng);
      op.str.resize(len);
      for (char &c : op.str) {
        c = static_cast<char>(ch_dist(rng));
      }
      break;
    }
    case OpType::Delete:
    case OpType::CopyRange: {
      std::uniform_int_distribution<ByteCount> count_dist(0, max_value - op.a);
      op.b = count_dist(rng);
      break;
    }
  }

  return op;
}

[[maybe_unused]] void CheckAtAgreement(const GapBuffer &actual,
                                       const ReferenceBuffer &expected,
                                       size_t pos) {
  const auto expected_at = expected.At(pos);
  if (expected_at) {
    EXPECT_EQ(actual.At(pos), *expected_at);
  }
}

[[maybe_unused]] void CheckEquivalentState(const GapBuffer &actual,
                                           const ReferenceBuffer &expected) {
  EXPECT_EQ(actual.Len(), expected.Len());
  EXPECT_EQ(Snapshot(actual), Snapshot(expected));

  const size_t len = expected.Len();
  const std::array<size_t, 3> positions{0, len == 0 ? 0 : len - 1, len / 2};
  for (size_t pos : positions) {
    CheckAtAgreement(actual, expected, pos);
  }
}

#ifdef CONTRACT_EXCEPTIONS
template <typename F>
void ExpectPreconditionViolation(F &&fn) {
  try {
    fn();
    FAIL() << "Expected std::logic_error";
  } catch (const std::logic_error &e) {
    EXPECT_NE(std::string(e.what()).find("PRECONDITION"), std::string::npos);
  }
}
#endif
}  // namespace

TEST(GapBufferTest, InsertBytesAndReadBack) {
  GapBuffer buffer;
  buffer.Insert(0, AsByteSpan("h"));
  buffer.Insert(1, AsByteSpan("e"));
  buffer.Insert(2, AsByteSpan("l"));
  buffer.Insert(3, AsByteSpan("l"));
  buffer.Insert(4, AsByteSpan("o"));

  EXPECT_EQ(buffer.Len(), 5);
  EXPECT_EQ(Snapshot(buffer), "hello");
  EXPECT_EQ(buffer.At(0), 'h');
  EXPECT_EQ(buffer.At(4), 'o');
}

TEST(GapBufferTest, InsertAtSpecificPositionMaintainsLogicalOrder) {
  GapBuffer buffer;
  buffer.Insert(0, AsByteSpan("hello"));

  buffer.Insert(0, AsByteSpan("X"));
  EXPECT_EQ(Snapshot(buffer), "Xhello");

  buffer = GapBuffer{};
  buffer.Insert(0, AsByteSpan("hello"));
  buffer.Insert(2, AsByteSpan("X"));
  EXPECT_EQ(Snapshot(buffer), "heXllo");
}

TEST(GapBufferTest, DeleteCharacterAtPosition) {
  GapBuffer buffer;
  buffer.Insert(0, AsByteSpan("abc"));

  buffer.Delete(2, 1);
  EXPECT_EQ(Snapshot(buffer), "ab");

  buffer = GapBuffer{};
  buffer.Insert(0, AsByteSpan("abc"));
  buffer.Delete(0, 1);
  EXPECT_EQ(Snapshot(buffer), "bc");

  buffer = GapBuffer{};
  buffer.Insert(0, AsByteSpan("abc"));
  buffer.Delete(1, 2);
  EXPECT_EQ(Snapshot(buffer), "a");
}

TEST(GapBufferTest, StrictBoundsCheckForInsertAndDelete) {
  GapBuffer buffer;
  buffer.Insert(0, AsByteSpan("abc"));

#ifdef CONTRACT_EXCEPTIONS
  ExpectPreconditionViolation([&] { buffer.Insert(4, AsByteSpan("x")); });
  ExpectPreconditionViolation([&] { buffer.Delete(4, 1); });
  ExpectPreconditionViolation([&] { buffer.Delete(0, 4); });
#else
  EXPECT_DEATH({ buffer.Insert(4, AsByteSpan("x")); }, "PRECONDITION FAILED");
  EXPECT_DEATH({ buffer.Delete(4, 1); }, "PRECONDITION FAILED");
  EXPECT_DEATH({ buffer.Delete(0, 4); }, "PRECONDITION FAILED");
#endif
}

TEST(GapBufferTest, AppendRangeCopiesRequestedLogicalSlice) {
  GapBuffer buffer;
  buffer.Insert(0, AsByteSpan("56789"));
  buffer.Insert(0, AsByteSpan("01234"));

  std::vector<Byte> out{'x'};
  const size_t copied = buffer.AppendRange(3, 4, out);

  EXPECT_EQ(copied, 4);
  EXPECT_EQ(std::string(out.begin(), out.end()), "x3456");
}

TEST(GapBufferTest, AppendRangeTruncatesToRemaining) {
  GapBuffer buffer;
  buffer.Insert(0, AsByteSpan("0123456789"));

  std::vector<Byte> out{'x'};
  const size_t copied = buffer.AppendRange(5, 100, out);

  EXPECT_EQ(copied, 5);
  EXPECT_EQ(std::string(out.begin(), out.end()), "x56789");
}

TEST(GapBufferTest, GrowthPreservesFullLogicalContent) {
  GapBuffer buffer;
  std::string expected;
  expected.reserve(1024);

  for (size_t i = 0; i < 1024; ++i) {
    const std::string ch(1, static_cast<char>('a' + (i % 26)));
    buffer.Insert(buffer.Len(), AsByteSpan(ch));
    expected += ch;
  }

  EXPECT_EQ(buffer.Len(), expected.size());
  EXPECT_EQ(Snapshot(buffer), expected);
}

TEST(GapBufferTest, BulkStringInsertSupportsLargeAndPositionalCases) {
  GapBuffer buffer;

  const std::string long_str(1024, 'x');
  buffer.Insert(0, AsByteSpan(long_str));
  EXPECT_EQ(buffer.Len(), long_str.size());
  EXPECT_EQ(Snapshot(buffer), long_str);

  buffer = GapBuffer{};
  buffer.Insert(0, AsByteSpan("Hello, "));
  buffer.Insert(0, AsByteSpan("world: "));
  EXPECT_EQ(Snapshot(buffer), "world: Hello, ");
}

TEST(GapBufferTest, RandomOperationStreamsStayEquivalentToReferenceModel) {
  constexpr uint64_t kDefaultSeed = 424242;
  constexpr size_t kSeedsToRun = 12;
  constexpr size_t kOpsPerSeed = 500;

  const uint64_t base_seed = SeedFromEnvOr(kDefaultSeed);

  for (size_t seed_offset = 0; seed_offset < kSeedsToRun; ++seed_offset) {
    const uint64_t seed = base_seed + seed_offset;
    SCOPED_TRACE("seed=" + std::to_string(seed));

    std::mt19937_64 rng(seed);
    GapBuffer actual;
    ReferenceBuffer expected;

    CheckEquivalentState(actual, expected);

    for (size_t op_index = 0; op_index < kOpsPerSeed; ++op_index) {
      const Op op = GenerateRandomOp(rng, expected.Len());
      SCOPED_TRACE("seed=" + std::to_string(seed) + ", op_index=" +
                   std::to_string(op_index) + ", op=" + DescribeOp(op));

      switch (op.type) {
        case OpType::InsertString:
          actual.Insert(op.a, AsByteSpan(op.str));
          expected.Insert(op.a, AsByteSpan(op.str));
          break;
        case OpType::Delete:
          actual.Delete(op.a, op.b);
          expected.Delete(op.a, op.b);
          break;
        case OpType::CopyRange: {
          std::vector<Byte> actual_out{'#'};
          std::vector<Byte> expected_out{'#'};
          const size_t actual_copied = actual.AppendRange(op.a, op.b, actual_out);
          const size_t expected_copied =
              expected.CopyRange(op.a, op.b, expected_out);
          EXPECT_EQ(actual_copied, expected_copied);
          EXPECT_EQ(actual_out, expected_out);
          break;
        }
      }

      CheckEquivalentState(actual, expected);
    }
  }
}

TEST(GapBufferTest, TargetedStressSequenceRemainsEquivalentToReferenceModel) {
  GapBuffer actual;
  ReferenceBuffer expected;

  const auto run = [&](const Op &op) {
    SCOPED_TRACE("op=" + DescribeOp(op));
    switch (op.type) {
      case OpType::InsertString:
        actual.Insert(op.a, AsByteSpan(op.str));
        expected.Insert(op.a, AsByteSpan(op.str));
        break;
      case OpType::Delete:
        actual.Delete(op.a, op.b);
        expected.Delete(op.a, op.b);
        break;
      case OpType::CopyRange: {
        std::vector<Byte> actual_out{'*'};
        std::vector<Byte> expected_out{'*'};
        EXPECT_EQ(actual.AppendRange(op.a, op.b, actual_out),
                  expected.CopyRange(op.a, op.b, expected_out));
        EXPECT_EQ(actual_out, expected_out);
        break;
      }
    }
    CheckEquivalentState(actual, expected);
  };

  for (size_t i = 0; i < 60; ++i) {
    if (expected.Len() == 0) {
      break;
    }
    run(Op{.type = OpType::Delete, .a = expected.Len() / 3, .b = 1});
  }

  run(Op{
      .type = OpType::InsertString, .a = expected.Len() / 2, .str = "HELLO"});

  run(Op{.type = OpType::CopyRange, .a = 0, .b = expected.Len()});
  run(Op{.type = OpType::Delete, .a = 0, .b = expected.Len()});
}

TEST(GapBufferTest, ConstructFromVectorTransfersContentAndSupportsEdits) {
  std::vector<Byte> source{'h', 'e', 'l', 'l', 'o'};
  const size_t source_len = source.size();
  GapBuffer buffer{std::move(source)};

  EXPECT_EQ(buffer.Len(), source_len);
  EXPECT_EQ(Snapshot(buffer), "hello");

  buffer.Insert(0, AsByteSpan("X"));
  EXPECT_EQ(Snapshot(buffer), "Xhello");

  buffer.Delete(0, 1);
  EXPECT_EQ(Snapshot(buffer), "hello");

  buffer.Delete(0, 1);
  EXPECT_EQ(Snapshot(buffer), "ello");
}

TEST(GapBufferTest, ConstructFromEmptyVectorAllowsSubsequentInsert) {
  GapBuffer buffer{std::vector<Byte>{}};
  EXPECT_EQ(buffer.Len(), 0);

  buffer.Insert(0, ByteSpan{reinterpret_cast<const Byte *>("abc"), 3});
  EXPECT_EQ(Snapshot(buffer), "abc");
}

TEST(GapBufferTest, ReallocationFollowedByGapMovePreservesFullContent) {
  GapBuffer buffer;
  const std::string initial(32, 'a');
  buffer.Insert(0, AsByteSpan(initial));

  const std::string big_insert(64, 'b');
  buffer.Insert(buffer.Len(), AsByteSpan(big_insert));
  buffer.Insert(0, AsByteSpan("X"));

  const std::string expected = "X" + initial + big_insert;
  EXPECT_EQ(buffer.Len(), expected.size());
  EXPECT_EQ(Snapshot(buffer), expected);
}

#ifdef CONTRACT_EXCEPTIONS
TEST(GapBufferTest, ContractViolationsThrowInDebugBuilds) {
  GapBuffer buffer;
  buffer.Insert(0, AsByteSpan("hello"));

  ExpectPreconditionViolation([&]() { std::ignore = buffer.At(buffer.Len()); });
  ExpectPreconditionViolation([&]() { std::ignore = buffer.At(100); });
  ExpectPreconditionViolation(
      [&]() { buffer.Insert(buffer.Len() + 1, AsByteSpan("x")); });
  ExpectPreconditionViolation(
      [&]() { buffer.Insert(100, AsByteSpan("test")); });
  ExpectPreconditionViolation([&]() { buffer.Delete(buffer.Len() + 1, 1); });
  ExpectPreconditionViolation([&]() { buffer.Delete(0, buffer.Len() + 1); });

  std::vector<Byte> out;
  ExpectPreconditionViolation(
      [&]() { std::ignore = buffer.AppendRange(buffer.Len() + 1, 0, out); });

  out.clear();
  EXPECT_EQ(buffer.AppendRange(0, buffer.Len() + 1, out), buffer.Len());
  EXPECT_EQ(std::string(out.begin(), out.end()), "hello");

  out.clear();
  EXPECT_EQ(buffer.AppendRange(2, 10, out), static_cast<ByteCount>(3));
  EXPECT_EQ(std::string(out.begin(), out.end()), "llo");

  GapBuffer empty_buf;
  ExpectPreconditionViolation([&]() { std::ignore = empty_buf.At(0); });
  ExpectPreconditionViolation([&]() { empty_buf.Delete(0, 1); });
}
#endif
