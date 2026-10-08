#include "document/internal/gap_buffer/gap_buffer.h"

#include <algorithm>
#include <cstring>

#include "internal/size_limits.h"

using unplugged::internal::CheckedAdd;
using unplugged::internal::CheckedByteCount;
using unplugged::internal::CheckedByteGrowth;

GapBuffer::GapBuffer(std::vector<Byte> &&buffer)
    : data_{std::move(buffer)},
      left_{CheckedByteCount(data_.size(), kMaxDocumentBytes)},
      right_{left_} {
  DBC_GUARD_CLASS_INVARIANTS();
}

[[nodiscard]] unplugged::dbc::InvariantResult GapBuffer::check_invariants()
    const {
  DBC_INVARIANT(left_ <= right_,
                "gap left cannot exceed gap right. left_={}, right_={}", left_,
                right_);
  DBC_INVARIANT(
      right_ <= data_.size(),
      "gap right cannot exceed total buffer size. right_={}, size()={}", right_,
      data_.size());
  DBC_INVARIANT(data_.size() <= unplugged::internal::kMaxStorageBytes,
                "physical storage must fit in a byte index");
  DBC_INVARIANT(Len() <= kMaxDocumentBytes,
                "logical storage must respect the document-size limit");
  return {};
}

Byte GapBuffer::At(ByteIndex pos) const
    noexcept(!kGapBufferContractExceptionsEnabled) {
  DBC_PRE(pos < Len(),
          "position must be less than buffer logical length. pos={}, Len()={}",
          pos, Len());

  if (pos < left_) {
    return data_[pos];
  }

  return data_[pos + GapSize()];
}

void GapBuffer::MoveGapTo(ByteIndex pos) noexcept(
    !kGapBufferContractExceptionsEnabled) {
  DBC_PRE(pos <= Len(),
          "position cannot exceed buffer logical length. pos={}, Len()={}", pos,
          Len());

  if (pos == left_) {
    return;
  }
  const ByteCount gapSize = GapSize();
  if (pos < left_) {
    std::copy_backward(data_.begin() + pos, data_.begin() + left_,
                       data_.begin() + left_ + gapSize);
  } else {
    std::copy(data_.begin() + right_, data_.begin() + pos + gapSize,
              data_.begin() + left_);
  }
  left_ = pos;
  right_ = pos + gapSize;
}

void GapBuffer::GrowGap(ByteCount needed) {
  if (GapSize() >= needed) {
    return;
  }

  const ByteCount grow_size = unplugged::internal::GapGrowth(
      CheckedByteCount(data_.size(), unplugged::internal::kMaxStorageBytes),
      GapSize(), needed);

  data_.reserve(data_.size() + grow_size);
  data_.resize(data_.size() + grow_size);
  std::copy_backward(data_.begin() + right_, data_.end() - grow_size,
                     data_.end());
  right_ += grow_size;
}

void GapBuffer::Insert(ByteIndex pos, const ByteSpan bytes) {
  Replace(pos, 0, bytes);
}

void GapBuffer::Replace(ByteIndex pos, ByteCount delete_count,
                        ByteSpan insert_bytes) {
  DBC_PRE(pos <= Len(),
          "position cannot exceed buffer logical length. pos={}, Len()={}", pos,
          Len());
  DBC_GUARD_CLASS_INVARIANTS();

  DBC_PRE(delete_count <= Len() - pos,
          "cannot delete beyond the end of the buffer. pos={}, count={}, "
          "Len()={}",
          pos, delete_count, Len());
  CheckedByteGrowth(Len() - delete_count, insert_bytes.size(),
                    kMaxDocumentBytes);
  const ByteCount count =
      CheckedByteCount(insert_bytes.size(), kMaxDocumentBytes);
  if (count == 0 && delete_count == 0) {
    return;
  }

  // Deletion contributes space to the gap. Finish allocating before modifying
  // logical bytes; the remaining operations on Byte cannot throw.
  GrowGap(count > delete_count ? count - delete_count : 0);
  MoveGapTo(pos);
  right_ += delete_count;
  if (count != 0) {
    std::memcpy(data_.data() + left_, insert_bytes.data(), count);
  }
  left_ += count;
}

void GapBuffer::Delete(ByteIndex pos, ByteCount count) noexcept(
    !kGapBufferContractExceptionsEnabled) {
  DBC_PRE(pos <= Len(),
          "position cannot exceed buffer logical length. pos={}, Len()={}", pos,
          Len());
  DBC_PRE(
      count <= Len() - pos,
      "cannot delete beyond the end of the buffer. pos={}, count={}, Len()={}",
      pos, count, Len());

  DBC_GUARD_CLASS_INVARIANTS();

  if (count == 0) {
    return;
  }

  MoveGapTo(pos);
  right_ += count;
}

ByteIndex GapBuffer::AppendRange(ByteIndex start, ByteCount count,
                                 std::vector<Byte> &out) const {
  DBC_PRE(start <= Len(),
          "position cannot exceed buffer logical length. start={}, Len()={}",
          start, Len());

  DBC_GUARD_CLASS_INVARIANTS();

  const ByteCount copied = std::min(count, Len() - start);
  const ByteIndex logicalEnd = start + copied;
  out.reserve(CheckedAdd(out.size(), copied, out.max_size()));

  ByteIndex pos = start;
  if (pos < left_) {
    const ByteIndex leftEnd = std::min(logicalEnd, left_);
    out.insert(out.end(), data_.begin() + pos, data_.begin() + leftEnd);
    pos = leftEnd;
  }

  if (pos < logicalEnd) {
    const ByteIndex physicalStart = pos + GapSize();
    const ByteIndex physicalEnd = physicalStart + (logicalEnd - pos);
    out.insert(out.end(), data_.begin() + physicalStart,
               data_.begin() + physicalEnd);
  }

  return copied;
}

ByteCount GapBuffer::GapSize() const noexcept { return right_ - left_; }

ByteCount GapBuffer::Len() const noexcept {
  return static_cast<ByteCount>(data_.size() - GapSize());
}

bool GapBuffer::IsValid(ByteIndex pos) const noexcept { return pos < Len(); }
