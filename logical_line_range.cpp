#include "logical_line_range.h"

#include <algorithm>

#include "document.h"
#include "document_view.h"

LogicalLineRange::iterator::iterator(const Document* doc, LineNumber line,
                                     LineNumber end_line) noexcept
    : doc_(doc), line_(line), end_line_(end_line) {}

DocumentView LogicalLineRange::iterator::operator*() const {
  const ByteIndex start = doc_->StartOfLine(line_);
  const ByteIndex end = (line_ + 1 < doc_->NumLines()) ? doc_->StartOfLine(line_ + 1)
                                                        : doc_->Len();
  return doc_->PartialView(start, end - start);
}

LogicalLineRange::iterator& LogicalLineRange::iterator::operator++() noexcept {
  ++line_;
  return *this;
}

LogicalLineRange::iterator LogicalLineRange::iterator::operator++(int) noexcept {
  auto tmp = *this;
  ++(*this);
  return tmp;
}

bool LogicalLineRange::iterator::operator==(const iterator& other) const noexcept {
  return doc_ == other.doc_ && line_ == other.line_ &&
         end_line_ == other.end_line_;
}

bool LogicalLineRange::iterator::operator!=(const iterator& other) const noexcept {
  return !(*this == other);
}

LogicalLineRange::LineNumber LogicalLineRange::iterator::line_number() const noexcept {
  return line_;
}

LogicalLineRange::LogicalLineRange(const Document& doc, LineNumber first,
                                   LineNumber end_exclusive) noexcept
    : doc_(&doc), first_(first), end_exclusive_(end_exclusive) {}

LogicalLineRange::iterator LogicalLineRange::begin() const noexcept {
  return iterator{doc_, first_, end_exclusive_};
}

LogicalLineRange::iterator LogicalLineRange::end() const noexcept {
  return iterator{doc_, end_exclusive_, end_exclusive_};
}

LogicalLineRange::LineNumber LogicalLineRange::first_line() const noexcept {
  return first_;
}

LogicalLineRange::LineNumber LogicalLineRange::end_line_exclusive() const noexcept {
  return end_exclusive_;
}

LogicalLineRange::LineCount LogicalLineRange::line_count() const noexcept {
  return end_exclusive_ - first_;
}

bool LogicalLineRange::empty() const noexcept { return first_ == end_exclusive_; }
