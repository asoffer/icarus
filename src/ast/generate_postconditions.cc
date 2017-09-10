#include "ast.h"

#include "../ir/property.h"
// TODO this approach is unsustainably complicated because it's essentially
// generating code directly from the AST. Instead it's better to build IR.

std::unique_ptr<IR::Func> ExprFn(AST::Expression* expr);

namespace AST {
void Identifier::GeneratePostconditions() const {
  if (decl->addr.value.is<IR::ReturnValue>()) {
    ASSERT_EQ(type, Bool);
    auto &postconditions =
        scope_->ContainingFnScope()->fn_lit->ir_func->postconditions_;
    postconditions[decl->addr.value.as<IR::ReturnValue>()] =
        std::make_unique<IR::property::BoolProperty>(true);
  } else {
    NOT_YET();
    // Deal with bad case
  }
}

void ChainOp::GeneratePostconditions() const {
  // TODO fix const_cast
  ExprFn(const_cast<ChainOp*>(this))->dump();
  NOT_YET();
}
} // namespace AST
