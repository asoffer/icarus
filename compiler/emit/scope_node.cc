#include <optional>
#include <utility>

#include "absl/strings/str_format.h"
#include "ast/ast.h"
#include "base/defer.h"
#include "compiler/compiler.h"
#include "ir/compiled_scope.h"
#include "ir/instruction/core.h"
#include "ir/instruction/inliner.h"
#include "ir/value/reg.h"
#include "ir/value/scope.h"
#include "ir/value/value.h"

namespace compiler {
namespace {

absl::flat_hash_map<std::string, core::Arguments<type::Typed<ir::Value>>>
InlineJumpIntoCurrent(ir::Builder &bldr, ir::Jump to_be_inlined,
                      absl::Span<ir::Value const> arguments,
                      ir::LocalBlockInterpretation const &block_interp) {
  auto const *jump           = ir::CompiledJump::From(to_be_inlined);
  auto *start_block          = bldr.CurrentBlock();
  size_t inlined_start_index = bldr.CurrentGroup()->blocks().size();

  auto *into = bldr.CurrentGroup();
  ir::InstructionInliner inl(jump, into, block_interp);

  bldr.CurrentBlock() = start_block;
  size_t i = 0;
  if (auto state_type = jump->type()->state()) {
    type::Apply(state_type, [&]<typename T>() {
      bldr.CurrentBlock()->Append(ir::RegisterInstruction<T>{
          .operand = arguments[i++].get<ir::Reg>(),
          .result  = bldr.CurrentGroup()->Reserve(),
      });
    });
  }

  for (auto const &p : jump->type()->params()) {
    type::Apply(p.value, [&]<typename T>() {
      bldr.CurrentBlock()->Append(ir::RegisterInstruction<T>{
          .operand = arguments[i++].get<ir::RegOr<T>>(),
          .result  = bldr.CurrentGroup()->Reserve(),
      });
    });
    // TODO Handle types not covered by Apply (structs, etc).
  }

  auto *entry = inl.InlineAllBlocks();

  bldr.CurrentBlock() = start_block;
  bldr.UncondJump(entry);
  return std::move(inl).ArgumentsByName();
}

struct BlockNodeResult {
  ir::BasicBlock * start;
  ir::BasicBlock *body;
  ir::Builder::BlockTerminationState termination;
};

BlockNodeResult EmitIrForBlockNode(Compiler &c, ast::BlockNode const *node,
                                   ir::LocalBlockInterpretation const &interp) {
  auto &bldr          = c.builder();
  auto *start         = interp[node];
  bldr.CurrentBlock() = start;
  auto *body =
      bldr.AddBlock(absl::StrFormat("body block for `%s`.", node->name()));
  bldr.UncondJump(body);

  bldr.CurrentBlock() = body;

  bldr.block_termination_state() =
      ir::Builder::BlockTerminationState::kMoreStatements;
  c.EmitValue(node);

  return BlockNodeResult{.start       = start,
                         .body        = body,
                         .termination = bldr.block_termination_state()};
}

std::optional<ir::Reg> InitializeScopeState(Compiler &c,
                                            ir::CompiledScope const &scope) {
  if (type::Type state_type = scope.state_type()) {
    ir::Reg state_ptr = c.builder().Alloca(state_type);
    c.EmitDefaultInit(type::Typed<ir::Reg>(state_ptr, state_type));
    return state_ptr;
  }
  return std::nullopt;
}

std::pair<ir::Jump, std::vector<ir::Value>> EmitIrForJumpArguments(
    Compiler &c, 
    core::Arguments<ast::Expression const *> const& args,
    ir::CompiledScope const &scope) {
  // TODO: Support dynamic dispatch.
  auto const &inits = scope.inits();
  ASSERT(inits.size() == 1u);
  auto &init = *inits.begin();
  // TODO: I'd rather assert that this is true.
  if (ir::CompiledJump::From(init)->work_item) {
    auto f = std::move(*ir::CompiledJump::From(init)->work_item);
    if (f) { std::move(f)(); }
  }

  auto arg_vals = args.Transform([&c](ast::Expression const *expr) {
    return type::Typed<ir::Value>(c.EmitValue(expr),
                                  c.context().qual_type(expr)->type());
  });
  return std::make_pair(
      init,
      c.PrepareCallArguments(
          ir::CompiledJump::From(init)->type()->state(),
          ir::CompiledJump::From(init)->params().Transform([](auto const &p) {
            return type::QualType::NonConstant(p.type());
          }),
          arg_vals));
}

}  // namespace

ir::Value Compiler::EmitValue(ast::ScopeNode const *node) {
  LOG("ScopeNode", "Emitting IR for ScopeNode");
  ir::Scope scope            = *EvaluateAs<ir::Scope>(node->name());
  auto const *compiled_scope = ir::CompiledScope::From(scope);

  // If the scope is stateful, stack-allocate space for the state and
  // default-initialize it.
  //
  // TODO: This interface isn't great beacuse it requires default-initializable
  // state and no way to call any other initializer.
  std::optional<ir::Reg> state_ptr =
      InitializeScopeState(*this, *compiled_scope);

  // Arguments to the scope's start must be re-evaluated on each call to `goto
  // start()`, so we need a block to which we can jump for this purpose.
  auto *args_block =
      builder().AddBlock(absl::StrFormat("args block for scope %p", node));

  // If a scope has a path that exits normally along some path (a call to exit
  // rather than a return or labelled yield), then this is the block on which we land.
  auto *landing_block =
      builder().AddBlock(absl::StrFormat("landing block for scope %p", node));

  // Insert empty phi-instructions for each return type. Any labeled yields will
  // inject their result values here.
  // TODO: Support more than one return type.
  // TODO: Support all types
  type::QualType const *qt = ASSERT_NOT_NULL(context().qual_type(node));
  std::optional<ir::Reg> result;
  if (qt->type() != type::Void) {
    type::ApplyTypes<bool, int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t,
                     uint32_t, uint64_t, float, double>(
        qt->type(), [&]<typename T>() {
          ir::PhiInstruction<T> phi;
          phi.result = builder().CurrentGroup()->Reserve();
          result     = phi.result;
          landing_block->Append(std::move(phi));
        });
  }

  // Push the scope landing state onto the the vector. Any nested scopes will be
  // able to lookup the label in this vector to determine where they should jump
  // to.
  state().scope_landings.push_back(TransientState::ScopeLandingState{
      .label       = node->label() ? node->label()->value() : ir::Label(),
      .scope       = scope,
      .result_type = *qt,
      .block       = landing_block,
  });
  base::defer d = [&] { state().scope_landings.pop_back(); };

  // Add new blocks
  absl::flat_hash_map<ast::BlockNode const *, ir::BasicBlock *> interp_map;
  for (auto const &block : node->blocks()) {
    interp_map.emplace(&block, builder().AddBlock(absl::StrCat(
                                   "Start of block `", block.name(), "`.")));
  }

  ir::LocalBlockInterpretation local_interp(std::move(interp_map), args_block,
                                            landing_block);


  builder().UncondJump(args_block);
  builder().CurrentBlock() = args_block;

  auto [init, args] =
      EmitIrForJumpArguments(*this, node->args(), *compiled_scope);
  if (state_ptr) { args.emplace(args.begin(), *state_ptr); }
  auto args_by_name =
      InlineJumpIntoCurrent(builder(), init, args, local_interp);
  for (auto const &[name, args] : args_by_name) {
    // TODO: Handle arguments for start/done blocks.
    if (name == "done" or name == "start") { continue; }
    builder().CurrentBlock() = local_interp[name];
    ast::BlockNode const *block_node = local_interp.block_node(name);

    auto *scope_block = ir::CompiledBlock::From(compiled_scope->block(name));
    ir::OverloadSet &before = scope_block->before();
    auto arg_types   = args.Transform([](type::Typed<ir::Value> const &v) {
      return type::QualType::NonConstant(v.type());
    });
    ir::Fn before_fn = before.Lookup(arg_types).value();
    LOG("ScopeNode", "%s: before %s", name, before_fn.type()->to_string());
    auto out_params = builder().OutParams(before_fn.type()->return_types());
    builder().Call(
        before_fn, before_fn.type(),
        PrepareCallArguments(nullptr, before_fn.type()->params(), args),
        out_params);
    ASSERT(out_params.size() == block_node->params().size());
    // TODO: This is probably incorrect.
    for (size_t i = 0; i < out_params.size(); ++i) {
      auto const *param = block_node->params()[i].value.get();
      auto addr         = builder().Alloca(context().qual_type(param)->type());
      context().set_addr(param, addr);
      type::Apply(before_fn.type()->params()[i].value.type(),
                  [&]<typename T>() {
                    builder().Store(ir::RegOr<T>(out_params[i]), addr);
                  });
    }
  }

  for (auto const &block_node : node->blocks()) {
    auto const *scope_block =
        ir::CompiledBlock::From(compiled_scope->block(block_node.name()));

    auto [start_block, body_block, termination] =
        EmitIrForBlockNode(*this, &block_node, local_interp);

    switch (termination) {
      case ir::Builder::BlockTerminationState::kMoreStatements: UNREACHABLE();
      case ir::Builder::BlockTerminationState::kNoTerminator: {
        auto const &afters = scope_block->after();
        // TODO: Choose the right jump.
        ASSERT(afters.size() == 1u);
        auto &after = *afters.begin();
        // TODO: I'd rather assert that this is true.
        if (ir::CompiledJump::From(after)->work_item) {
          auto f = std::move(*ir::CompiledJump::From(after)->work_item);
          if (f) { std::move(f)(); }
        }

        std::vector<ir::Value> after_args;
        if (state_ptr) { after_args.emplace(after_args.begin(), *state_ptr); }
        auto args_by_name =
            InlineJumpIntoCurrent(builder(), after, after_args, local_interp);
        for (auto const &[name, args] : args_by_name) {
          // TODO: Handle arguments for start/done blocks.
          if (name == "done" or name == "start") { continue; }
          builder().CurrentBlock() = local_interp[name];
          ast::BlockNode const *block_node = local_interp.block_node(name);

          auto *scope_block =
              ir::CompiledBlock::From(compiled_scope->block(name));
          ir::OverloadSet &before = scope_block->before();
          auto arg_types = args.Transform([](type::Typed<ir::Value> const &v) {
            return type::QualType::NonConstant(v.type());
          });
          ir::Fn before_fn = before.Lookup(arg_types).value();
          LOG("ScopeNode", "%s: before %s", name,
              before_fn.type()->to_string());
          auto out_params =
              builder().OutParams(before_fn.type()->return_types());
          builder().Call(
              before_fn, before_fn.type(),
              PrepareCallArguments(nullptr, before_fn.type()->params(), args),
              out_params);
          ASSERT(out_params.size() == block_node->params().size());
          // TODO: This is probably incorrect.
          for (size_t i = 0; i < out_params.size(); ++i) {
            auto const *param = block_node->params()[i].value.get();
            auto addr = builder().Alloca(context().qual_type(param)->type());
            context().set_addr(param, addr);
            type::Apply(before_fn.type()->params()[i].value.type(),
                        [&]<typename T>() {
                          builder().Store(ir::RegOr<T>(out_params[i]), addr);
                        });
          }
        }

      } break;
      case ir::Builder::BlockTerminationState::kGoto: 
      case ir::Builder::BlockTerminationState::kReturn: 
      case ir::Builder::BlockTerminationState::kLabeledYield:
      case ir::Builder::BlockTerminationState::kYield: continue;
    }
  }

  builder().CurrentBlock() = landing_block;
  builder().block_termination_state() =
      ir::Builder::BlockTerminationState::kMoreStatements;
  return result ? ir::Value(*result) : ir::Value();
}

void Compiler::EmitCopyInit(
    ast::ScopeNode const *node,
    absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to) {
  // TODO: Implement this properly.
  EmitAssign(node, to);
}

void Compiler::EmitMoveInit(
    ast::ScopeNode const *node,
    absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to) {
  // TODO: Implement this properly.
  EmitAssign(node, to);
}
void Compiler::EmitAssign(
    ast::ScopeNode const *node,
   absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to) {
  // TODO: Implement this properly.
  auto t = context().qual_type(node)->type();
  ASSERT(to.size() == 1u);
  EmitCopyAssign(to[0], type::Typed<ir::Value>(EmitValue(node), t));
}

}  // namespace compiler
