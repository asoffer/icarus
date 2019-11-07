#ifndef ICARUS_TYPE_TUPLE_H
#define ICARUS_TYPE_TUPLE_H

#include "base/lazy.h"
#include "ir/any_func.h"
#include "type/type.h"

namespace type {
struct Tuple : public Type {
  Tuple() = delete;
  ~Tuple() {}
  Tuple(std::vector<Type const *> entries) : entries_(std::move(entries)) {}

#include ICARUS_TYPE_VISITOR_METHODS

  void WriteTo(std::string *result) const override;

  core::Bytes offset(size_t n, core::Arch const &arch) const;
  size_t size() const { return entries_.size(); }

  core::Bytes bytes(core::Arch const &arch) const override;
  core::Alignment alignment(core::Arch const &arch) const override;

  bool IsDefaultInitializable() const;
  bool IsCopyable() const;
  bool IsMovable() const;
  bool HasDestructor() const;

  std::vector<Type const *> entries_;

  base::lazy<ir::CompiledFn *> destroy_func_;
  base::lazy<ir::CompiledFn *> init_func_;
  base::lazy<ir::CompiledFn *> copy_assign_func_;
  base::lazy<ir::CompiledFn *> move_assign_func_;
};

Type const *Tup(std::vector<Type const *> entries);

}  // namespace type

#endif  // ICARUS_TYPE_TUPLE_H
