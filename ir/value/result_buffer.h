#ifndef ICARUS_IR_VALUE_RESULT_BUFFER_H
#define ICARUS_IR_VALUE_RESULT_BUFFER_H

#include <vector>

#include "base/meta.h"
#include "base/serialize.h"
#include "base/untyped_buffer.h"
#include "base/untyped_buffer_view.h"
#include "ir/value/addr.h"
#include "ir/value/reg_or.h"

namespace ir {
namespace internal_result_buffer {

struct Offset {
  uint32_t index : 31;
  uint32_t is_register : 1;
};

struct Writer {
  explicit Writer(base::untyped_buffer *buffer)
      : buffer_(*ASSERT_NOT_NULL(buffer)) {}

  void write_bytes(absl::Span<std::byte const> bytes) {
    buffer_.write(buffer_.size(), bytes.data(), bytes.size());
  }

 private:
  base::untyped_buffer &buffer_;
};

struct Reader {
  explicit Reader(base::untyped_buffer_view::const_iterator iter)
      : iter_(iter) {}
  absl::Span<std::byte const> read_bytes(size_t count) {
    absl::Span<std::byte const> span(iter_.raw(), count);
    iter_.skip(count);
    return span;
  }

 private:
  base::untyped_buffer_view::const_iterator iter_;
};

}  // namespace internal_result_buffer

struct PartialResultBuffer;
struct CompleteResultBuffer;
struct CompleteResultRef;

struct PartialResultRef {
  PartialResultRef() = default;

  template <typename T>
  auto get() const {
    if constexpr (base::meta<T> == base::meta<Reg>) {
      ASSERT(is_register_ == true);
      return Reg(view_.get<Reg>(0));
    } else {
      if (is_register_) {
        return RegOr<T>(view_.get<Reg>(0));
      } else {
        T t;
        internal_result_buffer::Reader r(view_.begin());
        base::Deserialize(r, t);
        return RegOr<T>(t);
      }
    }
  }

  CompleteResultRef AsComplete() const;

  template <typename I>
  void Inline(I const &inliner) {
    if (is_register()) {
      Reg r = get<Reg>();
      inliner.Inline(r);
      std::memcpy(const_cast<std::byte *>(view_.data()), &r, sizeof(r));
    }
  }

  bool empty() const { return view_.empty(); }
  base::untyped_buffer_view raw() const {
    return base::untyped_buffer_view(view_.data(), view_.size());
  }

  bool is_register() const { return is_register_; }

 private:
  friend PartialResultBuffer;
  friend CompleteResultRef;

  explicit PartialResultRef(base::untyped_buffer_view view, bool is_register)
      : view_(view), is_register_(is_register) {}

  base::untyped_buffer_view view_;
  bool is_register_;
};

struct CompleteResultRef {
  CompleteResultRef() = default;
  explicit CompleteResultRef(base::untyped_buffer_view view) : view_(view) {}

  template <typename T>
  T get() const {
    T t;
    internal_result_buffer::Reader r(view_.begin());
    base::Deserialize(r, t);
    return t;
  }

  bool empty() const { return view_.empty(); }
  base::untyped_buffer_view raw() const {
    return base::untyped_buffer_view(view_.data(), view_.size());
  }

  operator PartialResultRef() const { return PartialResultRef(view_, false); }

  friend absl::FormatConvertResult<absl::FormatConversionCharSet::kString>
  AbslFormatConvert(CompleteResultRef const &ref,
                    const absl::FormatConversionSpec &spec,
                    absl::FormatSink *s) {
    s->Append(base::UniversalPrintToString(ref.view_));
    return {true};
  }

  friend std::ostream &operator<<(std::ostream &os,
                                  CompleteResultRef const &ref) {
    absl::Format(&os, "%s", ref);
    return os;
  }

 private:
  friend PartialResultRef;
  friend CompleteResultBuffer;

  base::untyped_buffer_view view_;
};

struct CompleteResultBuffer {
  void reserve_bytes(size_t num_entries, size_t num_bytes);

  addr_t append_slot(size_t slot_size);

  void clear() {
    buffer_.clear();
    offsets_.clear();
  }

  ir::addr_t data() { return buffer_.data(); }

  void append(CompleteResultBuffer const &value);
  void append(CompleteResultRef value);

  template <typename T>
  void append(T const &value) {
    static_assert(not base::HasErasureWrapper<T>);
    offsets_.push_back(buffer_.size());
    internal_result_buffer::Writer w(&buffer_);
    base::Serialize(w, value);
  }

  void append();

  void append_raw(base::untyped_buffer_view data) {
    offsets_.push_back(buffer_.size());
    buffer_.write(buffer_.size(), data.data(), data.size());
  }

  template <typename S>
  friend void BaseSerialize(S &s, CompleteResultBuffer const &buffer) {
    base::Serialize(s, buffer.offsets_, buffer.buffer_.size());
    s.write_bytes(absl::Span<std::byte const>(buffer.buffer_.data(),
                                              buffer.buffer_.size()));
  }

  template <typename D>
  friend void BaseDeserialize(D &d, CompleteResultBuffer &buffer) {
    size_t num_bytes;
    base::Deserialize(d, buffer.offsets_, num_bytes);
    buffer.buffer_.write(0, d.read_bytes(num_bytes).data(), num_bytes);
  }

  base::untyped_buffer &&buffer() && { return std::move(buffer_); }
  base::untyped_buffer &buffer() & { return buffer_; }

  bool empty() const { return offsets_.empty(); }
  size_t num_entries() const { return offsets_.size(); }

  template <typename T>
  T get(size_t i) const {
    return (*this)[i].get<T>();
  }

  void pop_back() {
    ASSERT(offsets_.size() != 0);
    size_t index = offsets_.back();
    buffer_.resize(index);
    offsets_.pop_back();
  }

  CompleteResultRef operator[](size_t i) const;
  CompleteResultRef back() const;

 private:
  friend PartialResultBuffer;

  std::vector<size_t> offsets_;
  base::untyped_buffer buffer_;
};

struct PartialResultBuffer {
  PartialResultBuffer() = default;
  PartialResultBuffer(CompleteResultBuffer buffer);

  void clear() {
    buffer_.clear();
    offsets_.clear();
  }

  void append(PartialResultBuffer const &value);
  void append(CompleteResultBuffer const &value);
  void append(PartialResultRef value);
  void append(CompleteResultRef value) {
    append(static_cast<PartialResultRef>(value));
  }

  template <typename T>
  void append(T const &value) {
    if constexpr (base::meta<T>.template is_a<RegOr>()) {
      static_assert(not base::HasErasureWrapper<typename T::type>);
      offsets_.push_back(internal_result_buffer::Offset{
          .index       = static_cast<uint32_t>(buffer_.size()),
          .is_register = value.is_reg()});
      if (value.is_reg()) {
        buffer_.append(value.reg());
      } else {
        internal_result_buffer::Writer w(&buffer_);
        base::Serialize(w, value);
      }
    } else if constexpr (base::meta<T> == base::meta<Reg>) {
      offsets_.push_back(internal_result_buffer::Offset{
          .index = static_cast<uint32_t>(buffer_.size()), .is_register = true});
      buffer_.append(value);
    } else {
      static_assert(not base::HasErasureWrapper<T>);
      offsets_.push_back(internal_result_buffer::Offset{
          .index       = static_cast<uint32_t>(buffer_.size()),
          .is_register = false});
      internal_result_buffer::Writer w(&buffer_);
      base::Serialize(w, value);
    }
  }

  void append();

  template <typename S>
  friend void BaseSerialize(S &s, PartialResultBuffer const &buffer) {
    base::Serialize(s, buffer.offsets_, buffer.buffer_.size());
    s.write_bytes(absl::Span<std::byte const>(buffer.buffer_.data(),
                                              buffer.buffer_.size()));
  }

  template <typename D>
  friend void BaseDeserialize(D &d, PartialResultBuffer &buffer) {
    size_t num_bytes;
    base::Deserialize(d, buffer.offsets_, num_bytes);
    buffer.buffer_.write(0, d.read_bytes(num_bytes).data(), num_bytes);
  }

  bool is_register(size_t i) const { return offsets_[i].is_register; }

  base::untyped_buffer buffer() && { return std::move(buffer_); }

  bool empty() const { return offsets_.empty(); }
  size_t num_entries() const { return offsets_.size(); }

  template <typename T>
  auto get(size_t i) const {
    return (*this)[i].get<T>();
  }

  void pop_back() {
    size_t index = offsets_.back().index;
    buffer_.resize(index);
    offsets_.pop_back();
  }

  void set_register(size_t i, Reg r);

  PartialResultRef operator[](size_t i) const;
  PartialResultRef back() const;

  size_t size() const { return offsets_.size(); }

 private:
  std::vector<internal_result_buffer::Offset> offsets_;
  base::untyped_buffer buffer_;
};

}  // namespace ir

#endif  // ICARUS_IR_VALUE_RESULT_BUFFER_H
