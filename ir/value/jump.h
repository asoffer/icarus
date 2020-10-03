#ifndef ICARUS_IR_VALUE_JUMP_H
#define ICARUS_IR_VALUE_JUMP_H

#include <iostream>

namespace ir {
struct CompiledJump;

struct Jump {
  explicit constexpr Jump(CompiledJump const *jump = nullptr) : jump_(jump) {}

  friend bool operator==(Jump lhs, Jump rhs) { return lhs.jump_ == rhs.jump_; }
  friend bool operator!=(Jump lhs, Jump rhs) { return not(lhs == rhs); }

  template <typename H>
  friend H AbslHashValue(H h, Jump j) {
    return H::combine(std::move(h), j.jump_);
  }

  friend std::ostream &operator<<(std::ostream &os, Jump j) {
    return os << "Jump(" << j.jump_ << ")";
  }

 private:
  friend CompiledJump;
  CompiledJump const *jump_;
};

}  // namespace ir

#endif  // ICARUS_IR_VALUE_JUMP_H