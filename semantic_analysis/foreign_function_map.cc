#include "semantic_analysis/foreign_function_map.h"

#include <dlfcn.h>

#include "base/debug.h"

namespace semantic_analysis {
namespace {

IrFunction ConstructIrFunction(core::FunctionType fn_type, void (*fn_ptr)()) {
  auto const& parameters = fn_type.parameters();
  std::span returns      = fn_type.returns();
  IrFunction f(parameters.size(), returns.size());
  ASSERT(returns.size() <= 1);
  f.append<InvokeForeignFunction>(fn_ptr, parameters.data(), parameters.size(),
                                  returns.empty() ? nullptr : returns.data());
  f.append<jasmin::Return>();
  return f;
}

}  // namespace

std::pair<data_types::Fn, IrFunction const*>
ForeignFunctionMap::ForeignFunction(std::string name, core::FunctionType t) {
  auto [iter, inserted] = foreign_functions_.try_emplace(
      std::pair<std::string, core::FunctionType>(std::move(name), t),
      IrFunction(0, 0), nullptr);
  if (inserted) {
    auto& [ir_fn, fn_ptr] = iter->second;
    dlerror();  // Clear previous errors.
    // NOTE: This reinterpret_cast is not allowed according to the C++ Standard,
    // but is guaranteed to be correct on POSIX compliant systems.
    fn_ptr = reinterpret_cast<void (*)()>(
        dlsym(RTLD_DEFAULT, iter->first.first.c_str()));
    char const* error = dlerror();

    // TODO: Handle errors.
    if (error != nullptr) { NOT_YET(iter->first.first); }

    ASSERT(fn_ptr != nullptr);
    ir_fn = ConstructIrFunction(t, fn_ptr);
  }

  ASSERT(foreign_functions_.from_index(foreign_functions_.index(iter))
             .second.second != nullptr);

  ASSERT(foreign_functions_.from_index(foreign_functions_.index(iter))
             .second.second != nullptr);
  return std::pair(data_types::Fn(serialization::ModuleIndex::Builtin(),
                                  data_types::LocalFnId::Foreign(
                                      foreign_functions_.index(iter))),
                   &iter->second.first);
}

IrFunction const* ForeignFunctionMap::ForeignFunction(data_types::LocalFnId id) const {
  ASSERT(id.foreign());
  return &foreign_functions_.from_index(id.value()).second.first;
}

std::type_identity_t<void (*)()> ForeignFunctionMap::ForeignFunctionPointer(
    data_types::LocalFnId id) const {
  ASSERT(id.foreign());
  return foreign_functions_.from_index(id.value()).second.second;
}

}  // namespace semantic_analysis
