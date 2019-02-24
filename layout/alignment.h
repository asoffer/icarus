#ifndef ICARUS_LAYOUT_ALIGNMENT_H
#define ICARUS_LAYOUT_ALIGNMENT_H

namespace layout {

struct Alignment {
  constexpr explicit Alignment(size_t val) : value_(val) {}

  constexpr auto value() const { return value_; }

 private:
  size_t value_ = 0;
};

constexpr bool operator==(Alignment lhs, Alignment rhs) {
  return lhs.value() == rhs.value();
}

constexpr bool operator!=(Alignment lhs, Alignment rhs) { return !(lhs == rhs); }
constexpr bool operator<(Alignment lhs, Alignment rhs) {
  return lhs.value() < rhs.value();
}

constexpr bool operator<=(Alignment lhs, Alignment rhs) { return !(rhs < lhs); }
constexpr bool operator>(Alignment lhs, Alignment rhs) { return rhs < lhs; }
constexpr bool operator>=(Alignment lhs, Alignment rhs) { return !(lhs < rhs); }

inline std::ostream& operator<<(std::ostream& os, Alignment a) {
  return os << "align(" << a.value() << ")";
}

}  // namespace layout

#endif  // ICARUS_LAYOUT_ALIGNMENT_H
