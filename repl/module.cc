#include "repl/module.h"
#include "absl/strings/str_format.h"
#include "ast/ast.h"
#include "ast/expression.h"
#include "ast/node.h"
#include "base/log.h"
#include "base/ptr_span.h"
#include "ir/builder.h"
#include "ir/compiled_fn.h"

namespace repl {
namespace {

void ReplEval(ast::Expression const *expr, compiler::Compiler *compiler) {
  // TODO is nullptr for module okay here?
  ir::CompiledFn fn(type::Func({}, {}), {});
  ICARUS_SCOPE(ir::SetCurrent(&fn)) {
    ir::GetBuilder().CurrentBlock() = fn.entry();

    // TODO support multiple values computed simultaneously?
    auto expr_val = compiler->EmitValue(expr);
    if (compiler->diag().num_consumed() != 0) { return; }
    // TODO compiler->CompleteDeferredBodies();
    auto *expr_type = compiler->type_of(expr);
    if (expr_type != type::Void()) { NOT_YET(); }
    compiler->builder().ReturnJump();
  }

  interpretter::Execute(&fn, base::untyped_buffer(0), {});
}

}  // namespace

void Module::ProcessNodes(base::PtrSpan<ast::Node const> nodes,
                          diagnostic::DiagnosticConsumer &diag) {
  compiler::Compiler c({
      .builder             = ir::GetBuilder(),
      .data                = data(),
      .diagnostic_consumer = diag,
      .importer            = importer,
  });
  for (ast::Node const *node : nodes) {
    LOG("repl", "%s", node->DebugString());
    if (node->is<ast::Declaration>()) {
      auto *decl = &node->as<ast::Declaration>();

      {
        c.VerifyType(decl);
        c.EmitValue(decl);
        // TODO c.CompleteDeferredBodies();
        if (c.diag().num_consumed() != 0) { return; }
      }

    } else if (node->is<ast::Expression>()) {
      auto *expr = &node->as<ast::Expression>();
      ReplEval(expr, &c);
      absl::FPrintF(stderr, "\n");
    } else {
      NOT_YET(*node);
    }
  }
}

}  // namespace repl