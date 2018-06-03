#ifndef ICARUS_IR_INTERFACE_H
#define ICARUS_IR_INTERFACE_H

#include <map>

namespace AST {
struct BlockLiteral;
}  // namespace AST

namespace type {
struct Type;
}  // namespace type

namespace IR {
struct Interface {
  bool Matches(const type::Type*) {
    // TODO
    return true;
  }

  // TODO would prefer unordered but constraints from Val's variant make this
  // hard.
  std::map<std::string, const type::Type*> field_map_;
};
inline bool operator==(const Interface& lhs, const Interface& rhs) {
  return lhs.field_map_ == rhs.field_map_;
}

// TODO not really comparable. just for variant? :(
inline bool operator<(const Interface& lhs, const Interface& rhs) {
  return lhs.field_map_ < rhs.field_map_;
}
}  // namespace IR

#endif  // ICARUS_IR_INTERFACE_H
