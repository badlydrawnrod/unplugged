#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>

#include "document/internal/line_starts/line_starts.h"

class Document;
class DocumentView;

class LogicalLineRange {
 public:
  using LineNumber = LineStarts::LineNumber;
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

    LineNumber line_number() const noexcept;

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

  LineNumber first_line() const noexcept;
  LineNumber end_line_exclusive() const noexcept;
  LineCount line_count() const noexcept;
  bool empty() const noexcept;

 private:
  const Document* doc_ = nullptr;
  LineNumber first_ = 0;
  LineNumber end_exclusive_ = 0;
};
