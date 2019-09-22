#include "ast/ast.h"
#include "backend/eval.h"
#include "base/debug.h"
#include "frontend/source/file.h"
#include "ir/compiled_fn.h"
#include "misc/context.h"
#include "misc/module.h"
#include "visitor/assign_scope.h"
#include "visitor/emit_ir.h"
#include "visitor/verify_type.h"

namespace frontend {
std::vector<std::unique_ptr<ast::Node>> Parse(Source *src, ::Module *mod);
}  // namespace frontend

std::atomic<bool> found_errors = false;
ir::CompiledFn *main_fn        = nullptr;

// Once this function exits the file is destructed and we no longer have
// access to the source lines. All verification for this module must be done
// inside this function.
Module *CompileModule(Module *mod, std::filesystem::path const *path) {
  mod->path_ = ASSERT_NOT_NULL(path);

  // TODO log an error if this fails.
  ASSIGN_OR(return nullptr, frontend::FileSource src,
                   frontend::FileSource::Make(*mod->path_));
  mod->error_log_ = error::Log(&src);

  mod->statements_ = frontend::Parse(&src, mod);
  if (mod->error_log_.size() > 0) {
    mod->error_log_.Dump();
    found_errors = true;
    return mod;
  }

  {
    visitor::AssignScope visitor;
    for (auto &stmt : mod->statements_) {
      stmt->assign_scope(&visitor, &mod->scope_);
    }
  }

  visitor::TraditionalCompilation visitor(mod);
  for (auto const &stmt : mod->statements_) { stmt->VerifyType(&visitor); }

  if (visitor.context().num_errors() > 0) {
    // TODO Is this right?
    visitor.context().DumpErrors();
    found_errors = true;
    return mod;
  }

  for (auto const &stmt : mod->statements_) { stmt->EmitValue(&visitor); }
  visitor.CompleteDeferredBodies();

  if (visitor.context().num_errors() > 0) {
    // TODO Is this right?
    visitor.context().DumpErrors();
    found_errors = true;
    return mod;
  }

  for (auto const &stmt : mod->statements_) {
    if (auto const *decl = stmt->if_as<ast::Declaration>()) {
      if (decl->id() != "main") { continue; }
      auto f = backend::EvaluateAs<ir::AnyFunc>(
          type::Typed{decl->init_val(),
                      visitor.context().type_of(decl->init_val())},
          &visitor.context());
      ASSERT(f.is_fn() == true);
      auto ir_fn = f.func();

      // TODO check more than one?

      // TODO need to be holding a lock when you do this.
      main_fn = ir_fn;
    } else {
      continue;
    }
  }

  return mod;
}
