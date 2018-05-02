#ifndef ICARUS_AST_SCOPE_LITERAL_H
#define ICARUS_AST_SCOPE_LITERAL_H

#include "ast/expression.h"

namespace AST {
struct Declaration;

struct ScopeLiteral : public Expression {
  ~ScopeLiteral() override {}
  std::string to_string(size_t n) const override;
  void assign_scope(Scope *scope) override;
  void ClearIdDecls() override;
  void VerifyType(Context *) override;
  void Validate(Context *) override;
  void SaveReferences(Scope *scope, std::vector<IR::Val> *args) override;
  void ExtractReturns(std::vector<const Expression *> *) const override;
  void contextualize(
      const Node *correspondant,
      const std::unordered_map<const Expression *, IR::Val> &) override;

  ScopeLiteral *Clone() const override;

  IR::Val EmitIR(Context *) override;

  std::unique_ptr<Declaration> enter_fn, exit_fn;
  std::unique_ptr<Scope> body_scope;
};
}  // namespace AST

#endif  // ICARUS_AST_SCOPE_LITERAL_H
