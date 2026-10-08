#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>

#include "document/types/types.h"

#if defined(CONTRACT_EXCEPTIONS)
inline constexpr bool kWrappedRowRangeContractExceptionsEnabled = true;
#else
inline constexpr bool kWrappedRowRangeContractExceptionsEnabled = false;
#endif

class Document;
class DocumentView;

class WrappedRowRange {
 public:
  using RowIndex = uint32_t;

  class iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = DocumentView;
    using difference_type = std::ptrdiff_t;
    using reference = DocumentView;

    iterator() = default;
    iterator(const WrappedRowRange* range, RowIndex row) noexcept;

    DocumentView operator*() const;

    iterator& operator++() noexcept;
    iterator operator++(int) noexcept;

    bool operator==(const iterator& other) const noexcept;
    bool operator!=(const iterator& other) const noexcept;

    RowIndex GetRowIndex() const noexcept;

   private:
    const WrappedRowRange* range_ = nullptr;
    RowIndex row_ = 0;
  };

  WrappedRowRange(
      const Document& doc, DocumentView line,
      ByteCount width) noexcept(!kWrappedRowRangeContractExceptionsEnabled);

  iterator begin() const noexcept;
  iterator end() const noexcept;

  ByteCount Width() const noexcept;
  RowIndex NumRows() const noexcept;

 private:
  const Document* doc_ = nullptr;
  ByteIndex start_ = 0;
  ByteCount content_count_ = 0;
  ByteCount width_ = 1;
  RowIndex row_count_ = 0;
};
