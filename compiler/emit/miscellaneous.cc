#include "ast/ast.h"
#include "compiler/compiler.h"
#include "compiler/emit/common.h"
#include "ir/value/module_id.h"

namespace compiler {

void Compiler::EmitToBuffer(ast::Module const *node,
                            ir::PartialResultBuffer &out) {
  NOT_YET();
}

void Compiler::EmitToBuffer(ast::ArgumentType const *node,
                            ir::PartialResultBuffer &out) {
  out.append(context().arg_type(node->name()));
}

void Compiler::EmitToBuffer(ast::BuiltinFn const *node,
                            ir::PartialResultBuffer &out) {
  out.append(ir::Fn(node->value()));
}

void Compiler::EmitToBuffer(ast::Import const *node,
                            ir::PartialResultBuffer &out) {
  context().LoadConstant(node, out);
}

void Compiler::EmitToBuffer(ast::Label const *node,
                            ir::PartialResultBuffer &out) {
  out.append(node->value());
}

void Compiler::EmitToBuffer(ast::IfStmt const *node,
                            ir::PartialResultBuffer &out) {
  if (node->hashtags.contains(ir::Hashtag::Const)) {
    if (*EvaluateOrDiagnoseAs<bool>(&node->condition())) {
      EmitIrForStatements(*this, &node->true_scope(), node->true_block());
    } else if (node->has_false_block()) {
      EmitIrForStatements(*this, &node->false_scope(), node->false_block());
    }
  } else {
    auto *true_block  = builder().CurrentGroup()->AppendBlock();
    auto *false_block = node->has_false_block()
                            ? builder().CurrentGroup()->AppendBlock()
                            : nullptr;

    ir::RegOr<bool> condition = EmitAs<bool>(&node->condition());
    builder().CondJump(
        condition, true_block,
        false_block ? false_block : builder().landing(&node->true_scope()));

    builder().CurrentBlock() = true_block;
    EmitIrForStatements(*this, &node->true_scope(), node->true_block());

    if (node->has_false_block()) {
      builder().CurrentBlock() = false_block;
      EmitIrForStatements(*this, &node->false_scope(), node->false_block());
      builder().UncondJump(builder().landing(&node->true_scope()));
      builder().CurrentBlock() = builder().landing(&node->true_scope());
    }
  }
}

void Compiler::EmitToBuffer(ast::WhileStmt const *node,
                            ir::PartialResultBuffer &out) {
  auto start_block = builder().CurrentGroup()->AppendBlock();
  auto body_block  = builder().CurrentGroup()->AppendBlock();
  auto landing     = builder().CurrentGroup()->AppendBlock();

  builder().UncondJump(start_block);

  builder().CurrentBlock()  = start_block;
  ir::RegOr<bool> condition = EmitAs<bool>(&node->condition());
  builder().CondJump(condition, body_block, landing);

  builder().CurrentBlock() = body_block;
  EmitIrForStatements(*this, &node->body_scope(), node->body());
  builder().UncondJump(start_block);
  builder().CurrentBlock() = landing;
}

}  // namespace compiler
