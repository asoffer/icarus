#include "ast/ast.h"

namespace ast {

bool Access::IsDependent() const { return operand()->IsDependent(); }

bool ArgumentType::IsDependent() const { return true; }

bool ArrayLiteral::IsDependent() const {
  for (auto const *elem : elems()) {
    if (elem->IsDependent()) { return true; }
  }
  return false;
}

bool ArrayType::IsDependent() const {
  if (data_type()->IsDependent()) { return true; }
  for (auto const *length : lengths()) {
    if (length->IsDependent()) { return true; }
  }
  return false;
}

bool Binop::IsDependent() const {
  return lhs()->IsDependent() or rhs()->IsDependent();
}

bool BlockLiteral::IsDependent() const {
  for (auto const *b : before()) {
    if (b->IsDependent()) { return true; }
  }
  for (auto const *a : after()) {
    if (a->IsDependent()) { return true; }
  }
  return false;
}

bool BlockNode::IsDependent() const {
  for (auto const *stmt : stmts()) {
    if (stmt->IsDependent()) { return true; }
  }
  return false;
}

bool Jump::IsDependent() const {
  // TODO
  return false;
}

bool BuiltinFn::IsDependent() const { return false; }

bool Call::IsDependent() const {
  if (callee()->IsDependent()) { return true; }
  for (auto const *arg : args().pos()) {
    if (arg->IsDependent()) { return true; }
  }
  for (auto [name, arg] : args().named()) {
    if (arg->IsDependent()) { return true; }
  }
  return false;
}

bool Cast::IsDependent() const {
  return expr()->IsDependent() or type()->IsDependent();
}

bool ChainOp::IsDependent() const {
  for (auto const *expr : exprs()) {
    if (expr->IsDependent()) { return true; }
  }
  return false;
}

bool CommaList::IsDependent() const {
  for (auto const &expr : exprs_) {
    if (expr->IsDependent()) { return true; }
  }
  return false;
}

bool Declaration::IsDependent() const {
  if (auto *t = type_expr(); t and t->IsDependent()) { return true; }
  if (auto *i = init_val(); i and i->IsDependent()) { return true; }
  return false;
}

bool DesignatedInitializer::IsDependent() const {
  // TODO
  return type()->IsDependent();
}

bool EnumLiteral::IsDependent() const {
  // TODO
  return false;
}

bool FunctionLiteral::IsDependent() const {
  // TODO
  return false;
}

bool Identifier::IsDependent() const {
  // TODO
  return false;
}

bool Import::IsDependent() const {
  return operand()->IsDependent();
}

bool Index::IsDependent() const {
  return lhs()->IsDependent() or rhs()->IsDependent();
}

bool Goto::IsDependent() const {
  // TODO
  return false;
}

bool Label::IsDependent() const { return false; }

bool ReturnStmt::IsDependent() const {
  // TODO
  return false;
}

bool YieldStmt::IsDependent() const {
  // TODO
  return false;
}

bool ScopeLiteral::IsDependent() const {
  // TODO
  return false;
}

bool ScopeNode::IsDependent() const {
  // TODO
  return false;
}

bool ShortFunctionLiteral::IsDependent() const {
  // TODO
  return false;
}

bool StructLiteral::IsDependent() const {
  // TODO
  return false;
}

bool ParameterizedStructLiteral::IsDependent() const {
  // TODO
  return false;
}

bool StructType::IsDependent() const {
  // TODO
  return false;
}

bool Switch::IsDependent() const {
  // TODO
  return false;
}

bool Terminal::IsDependent() const { return false; }

bool Unop::IsDependent() const { return operand()->IsDependent(); }

}  // namespace ast