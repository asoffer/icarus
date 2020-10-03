#ifndef ICARUS_COMPILER_DISPATCH_JUMP_TABLE_H
#define ICARUS_COMPILER_DISPATCH_JUMP_TABLE_H

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "ast/ast.h"
#include "base/expected.h"
#include "compiler/dispatch/overload.h"
#include "core/fn_args.h"
#include "ir/local_block_interpretation.h"
#include "ir/value/jump.h"
#include "ir/value/value.h"
#include "type/qual_type.h"

namespace compiler {
struct Compiler;  // TODO move into it's own header.

struct JumpDispatchTable {
  static base::expected<JumpDispatchTable> Verify(
      type::Type const *state_type,
      absl::flat_hash_set<ir::Jump> const &jumps,
      core::FnArgs<type::QualType> const &args);

  // TODO long-term the `jump` parameter should read from `table_`.
  static absl::flat_hash_map<
      std::string_view,
      std::pair<ir::BasicBlock *, core::FnArgs<type::Typed<ir::Value>>>>
  EmitCallOneOverload(std::optional<ir::Reg> state_reg, ir::Jump jump,
                      Compiler *compiler,
                      core::FnArgs<type::Typed<ir::Value>> args,
                      ir::LocalBlockInterpretation const &block_interp);

  void EmitCall(Compiler *compiler, core::FnArgs<type::Typed<ir::Value>> args,
                ir::LocalBlockInterpretation const &block_interp);

  // private:
  absl::flat_hash_map<ir::Jump, internal::ExprData> table_;
};

}  // namespace compiler

#endif  // ICARUS_COMPILER_DISPATCH_JUMP_TABLE_H
