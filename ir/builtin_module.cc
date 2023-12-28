#include "ir/builtin_module.h"

#include <string_view>

#include "common/identifier.h"
#include "ir/global_function_registry.h"
#include "nth/utility/no_destructor.h"
#include "type/type.h"

namespace ic {
namespace {

nth::NoDestructor<IrFunction> FunctionOrPointer([] {
  IrFunction f(1, 1);
  f.append<TypeKind>();
  f.append<jasmin::Duplicate>();
  f.append<jasmin::Push>(type::Type::Kind::Function);
  f.append<jasmin::Equal<type::Type::Kind>>();
  f.append<jasmin::JumpIf>(10);
  f.append<jasmin::Push>(type::Type::Kind::Pointer);
  f.append<jasmin::Equal<type::Type::Kind>>();
  f.append<jasmin::JumpIf>(6);
  f.append<PushType>(type::Bottom);
  f.append<jasmin::Return>();
  f.append<jasmin::Drop>();
  f.append<PushType>(type::Unit);
  f.append<jasmin::Return>();
  return f;
}());

nth::NoDestructor<IrFunction> AsciiEncodeFn([] {
  IrFunction f(1, 1);
  f.append<AsciiEncode>();
  f.append<jasmin::Return>();
  return f;
}());

nth::NoDestructor<IrFunction> AsciiDecodeFn([] {
  IrFunction f(1, 1);
  f.append<AsciiDecode>();
  f.append<jasmin::Return>();
  return f;
}());

nth::NoDestructor<IrFunction> Foreign([] {
  IrFunction f(3, 1);
  f.append<RegisterForeignFunction>();
  f.append<jasmin::Return>();
  return f;
}());

nth::NoDestructor<IrFunction> Opaque([] {
  IrFunction f(0, 1);
  f.append<ConstructOpaqueType>();
  f.append<jasmin::Return>();
  return f;
}());

nth::NoDestructor<IrFunction> Arguments([] {
  IrFunction f(0, 2);
  f.append<LoadProgramArguments>();
  f.append<jasmin::Return>();
  return f;
}());

nth::NoDestructor<IrFunction> Slice([] {
  IrFunction f(2, 2);
  f.append<jasmin::Return>();
  return f;
}());

nth::NoDestructor<std::vector<std::string>> BuiltinNamesImpl;

}  // namespace

std::span<std::string const> BuiltinNames() { return *BuiltinNamesImpl; }

Module BuiltinModule() {
  uint32_t next_id = 0;
  Module m;

  auto Register = [&](std::string_view name, type::Type t,
                      IrFunction const& f) {
    m.Insert(Identifier(name),
             {.qualified_type = type::QualifiedType::Constant(t),
              .value          = {jasmin::Value(&f)}});
    global_function_registry.Register(
        FunctionId(ModuleId::Builtin(), LocalFunctionId(next_id++)), &f);
    BuiltinNamesImpl->emplace_back(name);
  };

  Register("opaque",
           type::Function(
               type::Parameters(std::vector<type::ParametersType::Parameter>{}),
               {type::Type_}),
           *Opaque);

  Register("arguments",
           type::Function(
               type::Parameters(std::vector<type::ParametersType::Parameter>{}),
               {type::Slice(type::Slice(type::Char))}),
           *Arguments);

  Register("ascii_encode",
           type::Function(
               type::Parameters(std::vector<type::ParametersType::Parameter>{
                   {.type = type::U8}}),
               {type::Char}),
           *AsciiEncodeFn);

  Register("ascii_decode",
           type::Function(
               type::Parameters(std::vector<type::ParametersType::Parameter>{
                   {.type = type::Char}}),
               {type::U8}),
           *AsciiDecodeFn);

  Register("slice",
           type::Function(
               type::Parameters(std::vector<type::ParametersType::Parameter>{
                   {.type = type::BufPtr(type::Char)}, {.type = type::U64}}),
               {type::Slice(type::Char)}),
           *Slice);

  Register(
      "foreign",
      type::Dependent(
          type::DependentTerm::Function(
              type::DependentTerm::Value(
                  TypeErasedValue(type::Type_, {type::Type_})),
              type::DependentTerm::Function(
                  type::DependentTerm::Call(
                      type::DependentTerm::DeBruijnIndex(0),
                      type::DependentTerm::Value(TypeErasedValue(
                          type::Family(type::Type_), {&*FunctionOrPointer}))),
                  type::DependentTerm::DeBruijnIndex(1))),
          type::DependentParameterMapping(
              {type::DependentParameterMapping::Index::Value(1),
               type::DependentParameterMapping::Index::Implicit()})),
      *Foreign);
  return m;
}

}  // namespace ic
