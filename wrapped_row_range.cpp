#include "wrapped_row_range.h"

#include "document.h"
#include "document_view.h"
#include "dsl.h"

namespace {
WrappedRowRange::RowIndex ComputeRowCount(ByteCount content_count,
                                          ByteCount width) {
  if (content_count == 0) {
    return 1;
  }

  return static_cast<WrappedRowRange::RowIndex>((content_count + width - 1) /
                                                width);
}

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
