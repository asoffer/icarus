#include "compiler/dispatch/scope_table.h"

#include "ast/expression.h"
#include "base/debug.h"
#include "compiler/compiler.h"
#include "compiler/dispatch/parameters_and_arguments.h"
#include "compiler/extract_jumps.h"
#include "core/fn_params.h"
#include "ir/builder.h"
#include "ir/components.h"
#include "ir/inliner.h"
#include "type/cast.h"
#include "type/type.h"
#include "type/variant.h"

namespace compiler {
namespace {

// TODO this is independent of scope literals and therefore should be moved back
// into verify_type.cc.
std::vector<core::FnArgs<VerifyResult>> VerifyBlockNode(
    Compiler *compiler, ast::BlockNode const *node) {
  compiler->Visit(node, VerifyTypeTag{});

  ExtractJumps extractor;
  for (auto const *stmt : node->stmts()) { extractor.Visit(stmt); }

  auto yields = extractor.jumps(ExtractJumps::Kind::Yield);
  // TODO this setup is definitely wrong because it doesn't account for
  // multiple yields correctly. For example,
  //
  // ```
  //  result: int32 | bool = if (cond) then {
  //    yield 3
  //  } else if (other_cond) then {
  //    yield 4
  //  } else {
  //    yield true
  //  }
  //  ```
  std::vector<core::FnArgs<VerifyResult>> result;
  for (auto *yield : yields) {
    auto &back = result.emplace_back();
    // TODO actually fill a fnargs
    std::vector<std::pair<ast::Expression const *, VerifyResult>>
        local_pos_yields;
    for (auto *yield_expr : yields[0]->as<ast::YieldStmt>().exprs()) {
      back.pos_emplace(
          *ASSERT_NOT_NULL(compiler->prior_verification_attempt(yield_expr)));
    }
  }
  return result;
}

void EmitCallOneOverload(ir::ScopeDef const *scope_def, Compiler *compiler,
                         ir::Jump const *jump,
                         core::FnArgs<type::Typed<ir::Results>> const &args,
                         ir::LocalBlockInterpretation const &block_interp) {
  auto arg_results = PrepareCallArguments(compiler, jump->params(), args);
  static_cast<void>(arg_results);
  // TODO pass arguments to inliner.

  auto &bldr                            = compiler->builder();
  ast::BlockNode const *next_block_node = ir::Inline(bldr, jump, block_interp);
  ir::BlockDef const *block_def = scope_def->block(next_block_node->name());

  // TODO make an overload set and call it appropriately.
  bldr.Call(block_def->before_[0], type::Func({}, {}), {});
  bldr.UncondJump(block_interp[next_block_node]);

  DEBUG_LOG("EmitCallOneOverload")(*bldr.CurrentGroup());
}

// Emits code which determines if a function with parameters `params` should be
// called with arguments `args`. It does this by looking for variants in `args`
// and testing the actually held type to see if it matches the corresponding
// parameter type. Note that the parameter type need not be identical. Rather,
// there must be a cast from the actual argument type to the parameter type
// (usually due to a cast such as `int64` casting to `int64 | bool`).
ir::RegOr<bool> EmitRuntimeDispatchOneComparison(
    ir::Builder &bldr,
    core::FnParams<type::Typed<ast::Declaration const *>> const &params,
    core::FnArgs<type::Typed<ir::Results>> const &args) {
  size_t i = 0;
  for (; i < args.pos().size(); ++i) {
    auto &arg     = args.pos()[i];
    auto *arg_var = arg.type()->if_as<type::Variant>();
    if (not arg_var) { continue; }
    auto runtime_type =
        ir::Load<type::Type const *>(bldr.VariantType(arg->get<ir::Addr>(0)));
    // TODO Equality isn't the right thing to check
    return bldr.Eq(runtime_type, params.at(i).value.type());
  }
  for (; i < params.size(); ++i) {
    auto const &param = params.at(i);
    auto *arg         = args.at_or_null(param.name);
    if (not arg) { continue; }  // Default arguments
    auto *arg_var = arg->type()->if_as<type::Variant>();
    if (not arg_var) { continue; }
    NOT_YET();
  }
  return ir::RegOr<bool>(false);
}

void EmitRuntimeDispatch(
    ir::Builder &bldr,
    absl::flat_hash_map<ir::Jump const *, ir::ScopeDef const *> const &table,
    absl::flat_hash_map<ir::Jump const *, ir::BasicBlock *> const
        &callee_to_block,
    core::FnArgs<type::Typed<ir::Results>> const &args) {
  // TODO This is a simple linear search through the table which is certainly a
  // bad idea. We can optimize it later. Likely the right way to do this is to
  // find a perfect hash of the function variants that produces an index into a
  // block table so we pay for a hash and a single indirect jump. This may be
  // harder if you remove variant and implement `overlay`.

  auto iter = table.begin();

  while (true) {
    auto const & [ jump, scope_def ] = *iter;
    ++iter;

    if (iter == table.end()) {
      bldr.UncondJump(callee_to_block.at(jump));
      break;
    }

    ir::RegOr<bool> match =
        EmitRuntimeDispatchOneComparison(bldr, jump->params(), args);
    bldr.CurrentBlock() =
        ir::EarlyExitOn<true>(callee_to_block.at(jump), match);
  }
}

}  // namespace

base::expected<ScopeDispatchTable> ScopeDispatchTable::Verify(
    Compiler *compiler, ast::ScopeNode const *node,
    absl::flat_hash_map<ir::Jump const *, ir::ScopeDef const *> inits,
    core::FnArgs<VerifyResult> const &args) {
  absl::flat_hash_map<ir::ScopeDef const *,
                      absl::flat_hash_map<ir::Jump const *, FailedMatch>>
      failures;
  ScopeDispatchTable table;
  table.scope_node_ = node;
  table.init_map_   = std::move(inits);
  for (auto[jump, scope] : table.init_map_) {
    auto result = MatchArgsToParams(jump->params(), args);
    if (not result) {
      failures[scope].emplace(jump, result.error());
    } else {
      table.tables_[scope].inits.emplace(jump, *result);
    }
  }

  auto expanded_fnargs = ExpandedFnArgs(args);
  expanded_fnargs.erase(
      std::remove_if(
          expanded_fnargs.begin(), expanded_fnargs.end(),
          [&](core::FnArgs<type::Type const *> const &fnargs) {
            for (auto const & [ scope_def, one_table ] : table.tables_) {
              for (auto const & [ init, params ] : one_table.inits) {
                if (core::IsCallable(
                        params, fnargs,
                        [](type::Type const *arg,
                           type::Typed<ast::Declaration const *> param) {
                          return type::CanCast(arg, param.type());
                        })) {
                  return true;
                }
              }
            }
            return false;
          }),
      expanded_fnargs.end());
  if (not expanded_fnargs.empty()) { NOT_YET("log an error"); }

  // If there are any scopes in this overload set that do not have blocks of the
  // corresponding names, we should exit.

  DEBUG_LOG("ScopeNode")("Num tables = ", table.tables_.size());
  for (auto & [ scope_def, one_table ] : table.tables_) {
    for (auto const &block : node->blocks()) {
      DEBUG_LOG("ScopeNode")
      ("Verifying dispatch for block `", block.name(), "`");
      auto const *block_def = scope_def->block(block.name());
      if (not block_def) { NOT_YET("log an error"); }
      auto block_results = VerifyBlockNode(compiler, &block);
      DEBUG_LOG("ScopeNode")("    ", block_results);
      if (block_results.empty()) {
        // There are no relevant yield statements
        DEBUG_LOG("ScopeNode")("    ... empty block results");

        ASSIGN_OR(
            continue,  //
            auto jump_table,
            JumpDispatchTable::Verify(compiler, node, block_def->after_, {}));
        bool success =
            one_table.blocks.emplace(&block, std::move(jump_table)).second;
        static_cast<void>(success);
        ASSERT(success == true);
      } else {
        for (auto const &fn_args : block_results) { NOT_YET(); }
      }
      DEBUG_LOG("ScopeNode")("    ... done.");
    }
  }

  return table;
}

ir::Results ScopeDispatchTable::EmitCall(
    Compiler *compiler,
    core::FnArgs<type::Typed<ir::Results>> const &args) const {
  DEBUG_LOG("ScopelDispatchTable")
  ("Emitting a table with ", init_map_.size(), " entries.");
  auto &bldr = compiler->builder();

  auto *landing_block_hack = bldr.AddBlock();

  if (init_map_.size() == 1) {
    // If there's just one entry in the table we can avoid doing all the work to
    // generate runtime dispatch code. It will amount to only a few
    // unconditional jumps between blocks which will be optimized out, but
    // there's no sense in generating them in the first place..
    auto const & [ jump, scope_def ] = *init_map_.begin();
    auto const &one_table            = tables_.begin()->second;

    auto block_interp = bldr.MakeLocalBlockInterpretation(scope_node_);
    EmitCallOneOverload(scope_def, compiler, jump, args, block_interp);
    // TODO handle results
    for (auto const & [ node, table ] : one_table.blocks) {
      bldr.CurrentBlock() = block_interp[node];

      compiler->Visit(node, EmitValueTag{});
      // TODO Jump, handling yields

      // TODO do this for real. The hack here is to just ignore the end jumps
      // and finish.
      bldr.UncondJump(landing_block_hack);
    }

  } else {
    auto *land_block     = bldr.AddBlock();
    auto callee_to_block = bldr.AddBlocks(init_map_);

    EmitRuntimeDispatch(bldr, init_map_, callee_to_block, args);

    // Add basic blocks for each block node in the scope (for each scope
    // definition which might be callable).
    absl::flat_hash_map<ir::ScopeDef const *, ir::LocalBlockInterpretation>
        block_interps;
    for (auto const & [ scope_def, _ ] : tables_) {
      auto[iter, success] = block_interps.emplace(
          scope_def, bldr.MakeLocalBlockInterpretation(scope_node_));
      static_cast<void>(success);
      ASSERT(success == true);
    }

    for (auto const & [ jump, scope_def ] : init_map_) {
      bldr.CurrentBlock() = callee_to_block[jump];
      // Argument preparation is done inside EmitCallOneOverload
      EmitCallOneOverload(scope_def, compiler, jump, args,
                          block_interps.at(scope_def));
    }
    // TODO handle results

    for (auto const & [ scope_def, one_table ] : tables_) {
      for (auto const & [ node, table ] : one_table.blocks) {
        DEBUG_LOG("EmitCall")(node->DebugString());
        bldr.CurrentBlock() = block_interps.at(scope_def)[node];
        compiler->Visit(node, EmitValueTag{});
        bldr.UncondJump(landing_block_hack);
      }
    }
  }

  bldr.CurrentBlock() = landing_block_hack;
  DEBUG_LOG("EmitCall")(*bldr.CurrentGroup());
  return ir::Results{};
}

}  // namespace compiler
