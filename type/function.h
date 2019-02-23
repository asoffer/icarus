#ifndef ICARUS_TYPE_FUNCTION_H
#define ICARUS_TYPE_FUNCTION_H

#include <cstring>
#include "type/callable.h"

#ifdef ICARUS_USE_LLVM
namespace llvm {
class FunctionType;
}  // namespace llvm
#endif  // ICARUS_USE_LLVM

namespace type {
struct GenericFunction : public Callable {
  GenericFunction() {}
  ~GenericFunction() override {}
  void WriteTo(std::string *result) const override {
    result->append("generic");
  }
  void EmitCopyAssign(const Type *from_type, ir::Results const &from,
                      ir::RegisterOr<ir::Addr> to, Context *ctx) const override;
  void EmitMoveAssign(const Type *from_type, ir::Results const &from,
                      ir::RegisterOr<ir::Addr> to, Context *ctx) const override;
  void EmitInit(ir::Register reg, Context *ctx) const override;
  void EmitDestroy(ir::Register reg, Context *ctx) const override;
  ir::Results PrepareArgument(const Type *t, const ir::Results &val,
                          Context *ctx) const override;
  void EmitRepr(ir::Results const &id_val, Context *ctx) const override;
  void defining_modules(
      std::unordered_set<::Module const *> *modules) const override;
  Cmp Comparator() const override;
};

struct Function : public Callable {
  TYPE_FNS(Function);
  Function(std::vector<const Type *> in, std::vector<const Type *> out)
      : input(std::move(in)), output(std::move(out)) {
    for (auto *t : input) { ASSERT(t != nullptr); }
    for (auto *t : output) { ASSERT(t != nullptr); }
  }

#ifdef ICARUS_USE_LLVM
  llvm::FunctionType *llvm_fn(llvm::LLVMContext &ctx) const;
#endif  // ICARUS_USE_LLVM

  std::vector<const Type *> input, output;
};

Function const *Func(std::vector<Type const *> in,
                     std::vector<Type const *> out);

}  // namespace type

#endif  // ICARUS_TYPE_FUNCTION_H
