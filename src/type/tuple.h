#ifndef ICARUS_TYPE_TUPLE_H
#define ICARUS_TYPE_TUPLE_H

#include "type.h"

#include "ir/val.h"  // TODO pass by reference to avoid this include

namespace type {
Type const *Tup(base::vector<Type const *> entries);

struct Tuple : public Type {
  Tuple() = delete;
  ~Tuple() {}
  Tuple(base::vector<Type const *> entries) : entries_(std::move(entries)) {}
  virtual char *WriteTo(char *buf) const;
  virtual size_t string_size() const;
  virtual void EmitAssign(const Type *from_type, ir::Val from, ir::Register to,
                          Context *ctx) const {
    UNREACHABLE();
  }
  virtual void EmitInit(ir::Register reg, Context *ctx) const { UNREACHABLE(); }
  virtual void EmitDestroy(ir::Register reg, Context *ctx) const {
    UNREACHABLE();
  }
  virtual ir::Val PrepareArgument(const Type *t, const ir::Val &val,
                                  Context *ctx) const {
    UNREACHABLE();
  }
  virtual void EmitRepr(ir::Val const &id_val, Context *ctx) const {
    UNREACHABLE();
  }
  virtual Cmp Comparator() const { UNREACHABLE(); }

  Type const *finalize() {
    auto *result = Tup(std::move(entries_));
    ASSERT(this != result);
    delete this;
    return result;
  }
#ifdef ICARUS_USE_LLVM
  virtual llvm::Type *llvm(llvm::LLVMContext &) const { UNREACHABLE(); }
#endif  // ICARUS_USE_LLVM

  base::vector<Type const *> entries_;
};  // namespace type
}  // namespace type

#endif  // ICARUS_TYPE_TUPLE_H
