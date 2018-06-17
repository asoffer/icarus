#ifndef ICARUS_AST_JUMP_H
#define ICARUS_AST_JUMP_H

#include "ast/node.h"

namespace IR {
void ReturnJump();
}  // namespace IR

namespace AST {
struct Jump : public Node {
  enum class Kind { Return };

  Jump(const TextSpan &span, Kind jump_type)
      : Node(span), jump_type(jump_type) {}
  ~Jump() override {}

  std::string to_string(size_t n) const override {
    switch (jump_type) {
      case Kind::Return: return "return";
    }
    UNREACHABLE();
  }

  void assign_scope(Scope *scope) override { scope_ = scope; }
  void VerifyType(Context *) override {}
  void Validate(Context *) override {}
  void SaveReferences(Scope *scope, std::vector<IR::Val> *) override {}
  void ExtractReturns(std::vector<const Expression *> *) const override {};
  void contextualize(
      const Node *,
      const std::unordered_map<const Expression *, IR::Val> &) override {}

  Jump *Clone() const override { return new Jump(*this); }
  IR::Val EmitIR(Context *) override {
    switch (jump_type) {
      case Kind::Return: IR::ReturnJump(); return IR::Val::None();
    }
    UNREACHABLE();
  }

  ExecScope *scope;
  Kind jump_type;
};
}  // namespace AST

#endif  // ICARUS_AST_JUMP_H
