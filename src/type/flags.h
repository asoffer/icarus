#ifndef ICARUS_TYPE_FLAGS_H
#define ICARUS_TYPE_FLAGS_H

#include "base/container/unordered_map.h"
#include "type.h"

namespace type {
struct Flags : public type::Type {
  TYPE_FNS(Flags);
  Flags(const std::string &name, base::vector<std::string> members);

  size_t IntValueOrFail(const std::string &str) const;
  ir::Val EmitLiteral(const std::string &member_name) const;

  // TODO combine "members" and "int_values" to save the double allocation of
  // strings.
  std::string bound_name;
  base::vector<std::string> members_;
  base::unordered_map<std::string, size_t> int_values;
};
}  // namespace type
#endif  // ICARUS_TYPE_FLAGS_H
