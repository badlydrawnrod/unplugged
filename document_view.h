#pragma once

#include <iterator>

#include "document.h"

class DocumentView {
 public:
  class iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = Byte;
    using difference_type = std::ptrdiff_t;
    using reference = Byte;

    iterator() = default;

    iterator(const Document* doc, ByteIndex index) noexcept
        : doc_(doc), index_(index) {}

    Byte operator*() const noexcept { return doc_->At(index_); }

    iterator& operator++() noexcept {
      ++index_;
      return *this;
    }

    iterator operator++(int) noexcept {
      auto tmp = *this;
      ++(*this);
      return tmp;
    }

    iterator& operator+=(difference_type n) noexcept {
      index_ += static_cast<ByteIndex>(n);
      return *this;
    }

    bool operator==(const iterator& other) const noexcept {
      return doc_ == other.doc_ && index_ == other.index_;
    }

    bool operator!=(const iterator& other) const noexcept {
      return !(*this == other);
    }

    ByteIndex DocumentIndex() const noexcept { return index_; }

   private:
    const Document* doc_ = nullptr;
    ByteIndex index_ = 0;
  };

  DocumentView() = default;

  DocumentView(const Document& doc, ByteIndex start, ByteCount count) noexcept
      : doc_(&doc), start_(start), count_(count) {}

  iterator begin() const noexcept { return iterator{doc_, start_}; }
  iterator end() const noexcept { return iterator{doc_, start_ + count_}; }

  ByteCount size() const noexcept { return count_; }
  bool empty() const noexcept { return count_ == 0; }

  Byte operator[](ByteIndex i) const noexcept { return doc_->At(start_ + i); }

  ByteIndex StartIndex() const noexcept { return start_; }
  ByteIndex EndIndex() const noexcept { return start_ + count_; }

 private:
  const Document* doc_ = nullptr;
  ByteIndex start_ = 0;
  ByteCount count_ = 0;
};

static_assert(std::forward_iterator<DocumentView::iterator>,
              "DocumentView::iterator should model a forward iterator.");
static_assert(std::ranges::range<DocumentView>,
              "DocumentView should model a range.");
static_assert(std::ranges::forward_range<DocumentView>,
              "DocumentView should model a forward range.");
