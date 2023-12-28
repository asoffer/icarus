#include "ir/foreign_function.h"

#include <dlfcn.h>

#include <string>
#include <string_view>

#include "common/resources.h"
#include "ir/function.h"
#include "ir/function_id.h"
#include "ir/global_function_registry.h"
#include "jasmin/core/instruction.h"
#include "type/function.h"
#include "type/primitive.h"

namespace ic {
namespace {

nth::flyweight_map<std::pair<size_t, type::FunctionType>,
                   std::pair<type::FunctionType, IrFunction>>
    foreign_functions;

nth::flyweight_map<std::pair<size_t, type::PointerType>,
                   std::pair<type::PointerType, void*>>
    foreign_pointers;

}  // namespace

nth::flyweight_map<std::pair<size_t, type::FunctionType>,
                   std::pair<type::FunctionType, IrFunction>> const&
AllForeignFunctions() {
  return foreign_functions;
}

nth::flyweight_map<std::pair<size_t, type::PointerType>,
                   std::pair<type::PointerType, void*>> const&
AllForeignPointers() {
  return foreign_pointers;
}

std::pair<type::FunctionType, IrFunction> const& LookupForeignFunction(
    LocalFunctionId id) {
  return foreign_functions.from_index(id.value()).second;
}

size_t ForeignFunctionIndex(std::string_view name, type::FunctionType t) {
  return foreign_functions.index(foreign_functions.find(
      std::make_pair(resources.StringLiteralIndex(name), t)));
}

IrFunction const& InsertForeignFunction(std::string_view name,
                                        type::FunctionType t, bool implement) {
  auto [iter, inserted] = foreign_functions.try_emplace(
      std::make_pair(resources.StringLiteralIndex(name), t), t,
      IrFunction(0, 0));
  if (not inserted) {
    return iter->second.second;
  }

  size_t jasmin_parameter_size = 0;
  size_t jasmin_return_size    = 0;
  for (auto const& p : *t.parameters()) {
    jasmin_parameter_size += type::JasminSize(p.type);
  }
  for (type::Type return_type : t.returns()) {
    jasmin_return_size += type::JasminSize(return_type);
  }
  auto& fn = iter->second.second;
  fn.~IrFunction();
  auto* p = new (&fn) IrFunction(jasmin_parameter_size, jasmin_return_size);

  if (implement) {
    dlerror();  // Clear existing errors.
    void* result      = dlsym(RTLD_DEFAULT, std::string(name).c_str());
    char const* error = dlerror();
    if (error != nullptr) { NTH_UNIMPLEMENTED("{}") <<= {error}; }

    auto const& parameters = fn.append<InvokeForeignFunction>(
        {.parameters = static_cast<uint32_t>(t.parameters().size()),
         .returns    = static_cast<uint32_t>(t.returns().size())},
        t, result);
  }
  p->append<jasmin::Return>();
  global_function_registry.Register(
      FunctionId(ModuleId::Foreign(),
                 LocalFunctionId(foreign_functions.index(iter))),
      p);
  return *p;
}

void* InsertForeignPointer(std::string_view name, type::PointerType t) {
  auto [iter, inserted] = foreign_pointers.try_emplace(
      std::make_pair(resources.StringLiteralIndex(name), t), t, nullptr);
  if (not inserted) {
    return iter->second.second;
  }

  dlerror();  // Clear existing errors.
  iter->second.second = dlsym(RTLD_DEFAULT, std::string(name).c_str());
  char const* error   = dlerror();
  if (error != nullptr) { NTH_UNIMPLEMENTED("{}") <<= {error}; }

  NTH_LOG("{}") <<= {iter->second.second};
  global_pointer_registry.Register(foreign_pointers.index(iter),
                                   iter->second.second);
  return iter->second.second;
}


size_t ForeignPointerIndex(std::string_view name, type::PointerType t) {
  return foreign_pointers.index(foreign_pointers.find(
      std::make_pair(resources.StringLiteralIndex(name), t)));
}

std::pair<type::PointerType, void*> const& LookupForeignPointer(size_t index) {
  return foreign_pointers.from_index(index).second;
}

}  // namespace ic
