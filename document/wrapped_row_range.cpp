#include "document/wrapped_row_range.h"

#include <limits>

#include "document/document.h"
#include "document/document_view.h"
#include "contracts/impl/checks.h"

namespace {
constexpr WrappedRowRange::RowIndex ComputeRowCount(ByteCount content_count,
                                                    ByteCount width) {
  if (content_count == 0) {
    return 1;
  }

  // Width is positive. Subtract first so rounding up cannot overflow, even
  // when the content length or width is at the byte-count maximum.
  return 1 + (content_count - 1) / width;
}

static_assert(std::numeric_limits<WrappedRowRange::RowIndex>::max() >=
              std::numeric_limits<ByteCount>::max());
constexpr ByteCount kMaxByteCount = std::numeric_limits<ByteCount>::max();
static_assert(ComputeRowCount(0, kMaxByteCount) == 1);
static_assert(ComputeRowCount(kMaxByteCount, 1) == kMaxByteCount);
static_assert(ComputeRowCount(kMaxByteCount, 2) == kMaxByteCount / 2 + 1);
static_assert(ComputeRowCount(kMaxByteCount, kMaxByteCount) == 1);
static_assert(ComputeRowCount(kMaxDocumentBytes, kMaxByteCount) == 1);

ByteCount ContentCountWithoutTrailingNewline(DocumentView line) {
  const ByteCount size = line.size();
  if (size == 0) {
    return 0;
  }

  return line[size - 1] == '\n' ? size - 1 : size;
}
}  // namespace

WrappedRowRange::iterator::iterator(const WrappedRowRange* range,
                                    RowIndex row) noexcept
    : range_(range), row_(row) {}

DocumentView WrappedRowRange::iterator::operator*() const {
  const ByteIndex offset = row_ * range_->width_;
  const ByteCount remaining = range_->content_count_ - offset;
  const ByteCount count = remaining < range_->width_ ? remaining : range_->width_;
  return range_->doc_->PartialView(range_->start_ + offset, count);
}

WrappedRowRange::iterator& WrappedRowRange::iterator::operator++() noexcept {
  ++row_;
  return *this;
}

WrappedRowRange::iterator WrappedRowRange::iterator::operator++(int) noexcept {
  auto tmp = *this;
  ++(*this);
  return tmp;
}

bool WrappedRowRange::iterator::operator==(const iterator& other) const noexcept {
  return range_ == other.range_ && row_ == other.row_;
}

bool WrappedRowRange::iterator::operator!=(const iterator& other) const noexcept {
  return !(*this == other);
}

WrappedRowRange::RowIndex WrappedRowRange::iterator::row_index() const noexcept {
  return row_;
}

WrappedRowRange::WrappedRowRange(const Document& doc, DocumentView line,
                                 ByteCount width)
    noexcept(!kWrappedRowRangeContractExceptionsEnabled)
    : doc_(&doc),
      start_(line.StartIndex()),
      content_count_(ContentCountWithoutTrailingNewline(line)),
      width_(width) {
  DBC_PRE(width > 0, "wrapped row width must be greater than zero. width={}",
          width);
  row_count_ = ComputeRowCount(content_count_, width_);
}

WrappedRowRange::iterator WrappedRowRange::begin() const noexcept {
  return iterator{this, 0};
}

WrappedRowRange::iterator WrappedRowRange::end() const noexcept {
  return iterator{this, row_count_};
}

ByteCount WrappedRowRange::width() const noexcept { return width_; }

WrappedRowRange::RowIndex WrappedRowRange::row_count() const noexcept {
  return row_count_;
}
