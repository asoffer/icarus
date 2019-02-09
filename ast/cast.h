#ifndef ICARUS_AST_CAST_H
#define ICARUS_AST_CAST_H

#include <memory>
#include <vector>

#include "ast/dispatch.h"
#include "ast/expression.h"

struct Scope;
struct Context;

namespace ast {
struct Cast : public Expression {
  ~Cast() override {}
  std::string to_string(size_t n) const override;
  void assign_scope(Scope *scope) override;
  VerifyResult VerifyType(Context *) override;
  void Validate(Context *) override;
  void ExtractJumps(JumpExprs *) const override;

  std::vector<ir::Val> EmitIR(Context *) override;
  std::vector<ir::RegisterOr<ir::Addr>> EmitLVal(Context *) override;

  std::unique_ptr<Expression> expr_, type_;
};

}  // namespace ast

#endif  // ICARUS_AST_CAST_H
