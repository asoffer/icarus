#ifndef ICARUS_FRONTEND_TOKEN_H
#define ICARUS_FRONTEND_TOKEN_H

#include <string>
#include "ast/node.h"
#include "frontend/operators.h"

namespace frontend {
// ast node used only for holding tokens which have been lexed but not yet
// parsed.
struct Token : public ast::Node {
  Token(const TextSpan &span = TextSpan(), std::string str = "")
      : Node(span), token(std::move(str)) {
#define OPERATOR_MACRO(name, symbol, prec, assoc)                              \
  if (token == symbol) {                                                       \
    op = Language::Operator::name;                                             \
    return;                                                                    \
  }
#include "operators.xmacro.h"
#undef OPERATOR_MACRO
    op = Language::Operator::NotAnOperator;
  }

  ~Token() override {}

  std::string to_string(size_t) const override { return "[" + token + "]\n"; }

  void assign_scope(Scope *) override { UNREACHABLE(); }
  type::Type const *VerifyType(Context *) override { UNREACHABLE(); }
  void Validate(Context *) override { UNREACHABLE(); }
  void ExtractJumps(ast::JumpExprs *) const override { UNREACHABLE(); }

  base::vector<ir::Val> EmitIR(Context *) override { UNREACHABLE(); }

  std::string token;
  Language::Operator op;
};

}  // namespace frontend

#endif  // ICARUS_FRONTEND_TOKEN_H
