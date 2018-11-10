#ifndef ICARUS_AST_FUNCTION_LITERAL_H
#define ICARUS_AST_FUNCTION_LITERAL_H

#include "ast/bound_constants.h"
#include "ast/declaration.h"
#include "ast/dispatch.h"
#include "ast/expression.h"
#include "ast/identifier.h"
#include "ast/statements.h"
#include "base/container/map.h"
#include "base/container/vector.h"
#include "ir/val.h"
#include "scope.h"

struct Module;

namespace IR {
struct Func;
}  // namespace IR

namespace AST {

struct FunctionLiteral : public Expression {
  // Represents a function with all constants bound to some value.
  FunctionLiteral() {}
  FunctionLiteral(FunctionLiteral &&) noexcept = default;
  ~FunctionLiteral() override {}

  std::string to_string(size_t n) const override;
  void assign_scope(Scope *scope) override;
  type::Type const *VerifyType(Context *) override;
  void Validate(Context *) override;
  void SaveReferences(Scope *scope, base::vector<IR::Val> *args) override;
  void ExtractJumps(JumpExprs *) const override;
  void contextualize(
      const Node *correspondant,
      const base::unordered_map<const Expression *, IR::Val> &) override;

  type::Type const *VerifyTypeConcrete(Context *);

  FunctionLiteral *Clone() const override;
  base::vector<IR::Val> EmitIR(Context *) override;
  base::vector<IR::Register> EmitLVal(Context *) override;

  void CompleteBody(Context *ctx);

  std::unique_ptr<FnScope> fn_scope;

  base::vector<std::unique_ptr<Declaration>> inputs;
  base::vector<std::unique_ptr<Expression>> outputs;
  std::unique_ptr<Statements> statements;

  // Maps the string name of the declared argument to it's index:
  // Example: (a: int, b: char, c: string) -> int
  //           a => 0, b => 1, c => 2
  base::unordered_map<std::string, size_t> lookup_;
  bool return_type_inferred_ = false;
  Module *module_            = nullptr;

  IR::Func *ir_func_ = nullptr;
};
}  // namespace AST

#endif  // ICARUS_AST_FUNCTION_LITERAL_H
