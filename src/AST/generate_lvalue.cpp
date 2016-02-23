#include "AST.h"

extern llvm::Module *global_module;
extern llvm::IRBuilder<> builder;

namespace data {
extern llvm::Value *const_uint(size_t n);
} // namespace data

namespace AST {
llvm::Value *Identifier::generate_lvalue(Scope *scope) { return alloc; }

llvm::Value *Unop::generate_lvalue(Scope *scope) {
  if (op == Language::Operator::At) { return operand->generate_code(scope); }

  return nullptr;
}

llvm::Value *ChainOp::generate_lvalue(Scope *) { return nullptr; }
llvm::Value *ArrayType::generate_lvalue(Scope *) { return nullptr; }
llvm::Value *ArrayLiteral::generate_lvalue(Scope *) { return nullptr; }
llvm::Value *Terminal::generate_lvalue(Scope *) { return nullptr; }
llvm::Value *FunctionLiteral::generate_lvalue(Scope *) { return nullptr; }
llvm::Value *Case::generate_lvalue(Scope *) { return nullptr; }
llvm::Value *Assignment::generate_lvalue(Scope *) { return nullptr; }
llvm::Value *Declaration::generate_lvalue(Scope *) { return nullptr; }
llvm::Value *TypeLiteral::generate_lvalue(Scope *) { return nullptr; }
llvm::Value *EnumLiteral::generate_lvalue(Scope *) { return nullptr; }

llvm::Value *Access::generate_lvalue(Scope *scope) {
  // Automatically pass through pointers
  auto etype  = operand->type;
  auto e_lval = operand->generate_lvalue(scope);

  while (etype->is_pointer()) {
    etype  = static_cast<Pointer *>(etype)->pointee;
    e_lval = scope->builder().CreateLoad(e_lval);
  }

  auto struct_type = static_cast<Structure *>(etype);
  return scope->builder().CreateGEP(
      e_lval, {data::const_uint(0), struct_type->field_num(member_name)});
}

llvm::Value *Binop::generate_lvalue(Scope *scope) {
  if (op == Language::Operator::Index && lhs->type->is_array()) {
    auto lhs_val  = lhs->generate_lvalue(scope);
    auto rhs_val  = rhs->generate_code(scope);
    auto load_ptr = scope->builder().CreateLoad(lhs_val);
    return scope->builder().CreateGEP(load_ptr, {rhs_val}, "array_idx");
  }
  return nullptr;
}

} // namespace AST
