#include "ast/ast.h"
#include "compiler/compiler.h"

namespace compiler {

ir::Value Compiler::EmitValue(ast::Terminal const *node) {
  return node->value();
}

// TODO: Unit tests
void Compiler::EmitCopyAssign(
    ast::Terminal const *node,
    absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to) {
  auto t = context().qual_type(node)->type();
  ASSERT(to.size() == 1u);
  EmitCopyAssign(to[0], type::Typed<ir::Value>(EmitValue(node), t));
}

// TODO: Unit tests
void Compiler::EmitMoveAssign(
    ast::Terminal const *node,
    absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to) {
  auto t = context().qual_type(node)->type();
  ASSERT(to.size() == 1u);
  EmitMoveAssign(to[0], type::Typed<ir::Value>(EmitValue(node), t));
}

// TODO: Unit tests
void Compiler::EmitCopyInit(
    ast::Terminal const *node,
   absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to) {
  auto t = context().qual_type(node)->type();
  ASSERT(to.size() == 1u);
  EmitCopyAssign(to[0], type::Typed<ir::Value>(EmitValue(node), t));
}

// TODO: Unit tests
void Compiler::EmitMoveInit(
    ast::Terminal const *node,
   absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to) {
  auto t = context().qual_type(node)->type();
  ASSERT(to.size() == 1u);
  EmitMoveAssign(to[0], type::Typed<ir::Value>(EmitValue(node), t));
}

}  // namespace compiler
