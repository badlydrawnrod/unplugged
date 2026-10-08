#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>

#include "document/line_number.h"

class Document;
class DocumentView;

class LogicalLineRange {
 public:
  using LineNumber = unplugged::document::LineNumber;
  using LineCount = uint32_t;

  class iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = DocumentView;
    using difference_type = std::ptrdiff_t;
    using reference = DocumentView;

    iterator() = default;
    iterator(const Document* doc, LineNumber line, LineNumber end_line) noexcept;

    DocumentView operator*() const;

    iterator& operator++() noexcept;
    iterator operator++(int) noexcept;

    bool operator==(const iterator& other) const noexcept;
    bool operator!=(const iterator& other) const noexcept;

    LineNumber GetLineNumber() const noexcept;

   private:
    const Document* doc_ = nullptr;
    LineNumber line_ = 0;
    LineNumber end_line_ = 0;
  };

  LogicalLineRange() = default;
  LogicalLineRange(const Document& doc, LineNumber first,
                   LineNumber end_exclusive) noexcept;

  iterator begin() const noexcept;
  iterator end() const noexcept;

  LineNumber FirstLine() const noexcept;
  LineNumber EndLineExclusive() const noexcept;
  LineCount NumLines() const noexcept;
  bool empty() const noexcept;

 private:
  const Document* doc_ = nullptr;
  LineNumber first_ = 0;
  LineNumber end_exclusive_ = 0;
};
