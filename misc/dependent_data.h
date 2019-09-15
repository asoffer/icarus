#ifndef ICARUS_MISC_DEPENDENT_DATA_H
#define ICARUS_MISC_DEPENDENT_DATA_H

#include "absl/container/flat_hash_map.h"
#include "absl/container/node_hash_map.h"
#include "ast/expr_ptr.h"
#include "misc/constant_binding.h"

// TODO this ifdef needs to disappear it's not long-term sustainable
#ifdef ICARUS_VISITOR_EMIT_IR
#include "ast/dispatch_table.h"
#include "ast/expr_ptr.h"
#include "ir/scope_def.h"
#include "visitor/verify_result.h"
#endif // ICARUS_VISITOR_EMIT_IR

namespace ast {
struct Declaration;
struct Expression;
}  // namespace ast

namespace ir {
struct CompiledFn;
}  // namespace ir

struct DependentData {
  absl::flat_hash_map<ast::Declaration const *, ir::Reg> addr_;

  // TODO probably make these funcs constant.
  absl::node_hash_map<ast::Expression const *, ir::CompiledFn *> ir_funcs_;

#ifdef ICARUS_VISITOR_EMIT_IR
  // TODO future optimization: the bool determining if it's const is not
  // dependent and can therefore be stored more efficiently (though querying
  // for both simultaneously would be more expensive I guess.
  absl::flat_hash_map<ast::ExprPtr, visitor::VerifyResult> verify_results_;

  // TODO this ifdef needs to disappear it's not long-term sustainable
  absl::flat_hash_map<ast::ExprPtr, ast::DispatchTable> dispatch_tables_;

  // Similar to dispatch tables, but specifically for `jump_handler`s. The
  // tables are keyed on both the scope/block node as well as the actual jump
  // expression.
  absl::flat_hash_map<std::pair<ast::ExprPtr, ast::ExprPtr>, ast::DispatchTable>
      jump_tables_;
  absl::node_hash_map<ast::ScopeLiteral const *, ir::ScopeDef> scope_defs_;

#endif
  ConstantBinding constants_;

  absl::flat_hash_map<ast::Import const *, core::PendingModule>
      imported_module_;
};

#endif  // ICARUS_MISC_DEPENDENT_DATA_H
