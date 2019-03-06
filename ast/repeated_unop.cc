#include "ast/repeated_unop.h"

#include "ast/fn_args.h"
#include "ast/function_literal.h"
#include "ast/overload_set.h"
#include "ir/func.h"
#include "misc/context.h"
#include "misc/scope.h"

namespace ast {
std::string RepeatedUnop::to_string(size_t n) const {
  switch (op_) {
    case frontend::Operator::Return: return "return " + args_.to_string(n);
    case frontend::Operator::Yield: return "yield " + args_.to_string(n);
    case frontend::Operator::Print: return "print " + args_.to_string(n);
    default: { UNREACHABLE(); }
  }
}

RepeatedUnop::RepeatedUnop(TextSpan const &text_span) {
  span = args_.span = text_span;
}

void RepeatedUnop::assign_scope(Scope *scope) {
  scope_ = scope;
  args_.assign_scope(scope);
}

void RepeatedUnop::DependentDecls(base::Graph<Declaration *> *g,
                                  Declaration *d) const {
  args_.DependentDecls(g, d);
}

void RepeatedUnop::ExtractJumps(JumpExprs *rets) const {
  args_.ExtractJumps(rets);
  // TODO yield as well?
  switch (op_) {
    case frontend::Operator::Return:
      (*rets)[JumpKind::Return].push_back(&args_);
      break;
    case frontend::Operator::Yield:
      (*rets)[JumpKind::Yield].push_back(&args_);
      break;
    default: break;
  }
}

VerifyResult RepeatedUnop::VerifyType(Context *ctx) {
  ASSIGN_OR(return _, auto result, args_.VerifyType(ctx));

  std::vector<type::Type const *> arg_types =
      result.type_->is<type::Tuple>()
          ? result.type_->as<type::Tuple>().entries_
          : std::vector<type::Type const *>{result.type_};

  if (op_ == frontend::Operator::Print) {
    // TODO what's the actual size given expansion of tuples and stuff?
    for (size_t i = 0; i < args_.exprs_.size(); ++i) {
      auto &arg      = args_.exprs_[i];
      auto *arg_type = arg_types[i];
      if (arg_type->is<type::Primitive>() || arg_type->is<type::Pointer>() ||
          arg_type == type::ByteView || arg_type->is<type::Enum>() ||
          arg_type->is<type::Flags>() || arg_type->is<type::Array>()) {
        continue;
      } else if (arg_type->is<type::Struct>()) {
        OverloadSet os(scope_, "print", ctx);
        os.add_adl("print", arg_type);

        ASSIGN_OR(return VerifyResult::Error(), type::Type const &ret_type,
                         DispatchTable::MakeOrLogError(
                             this, FnArgs<Expression *>({arg.get()}, {}), os,
                             ctx, true));
        if (&ret_type != type::Void()) { NOT_YET("log an error: ", &ret_type); }
      } else if (arg_type->is<type::Variant>()) {
        // TODO check that any variant can be printed
      } else if (arg_type->is<type::Tuple>()) {
        // TODO check that all tuple members can be printed.
      } else {
        NOT_YET(arg_type);
      }
    }
  }

  return VerifyResult(type::Void(), result.const_);
}

ir::Results RepeatedUnop::EmitIr(Context *ctx) {
  std::vector<ir::Results> arg_vals;
  if (args_.needs_expansion()) {
    for (auto &expr : args_.exprs_) {
      auto vals = expr->EmitIr(ctx);
      for (size_t i = 0; i < vals.size(); ++i) {
        arg_vals.push_back(vals.GetResult(i));
      }
    }
  } else {
    auto vals = args_.EmitIr(ctx);
    for (size_t i = 0; i < vals.size(); ++i) {
      arg_vals.push_back(vals.GetResult(i));
    }
  }

  switch (op_) {
    case frontend::Operator::Return: {
      size_t offset  = 0;
      auto *fn_scope = ASSERT_NOT_NULL(scope_->ContainingFnScope());
      auto *fn_lit   = ASSERT_NOT_NULL(fn_scope->fn_lit_);

      auto *fn_type =
          &ASSERT_NOT_NULL(ctx->type_of(fn_lit))->as<type::Function>();
      for (size_t i = 0; i < arg_vals.size(); ++i) {
        // TODO return type maybe not the same as type actually returned?
        ir::SetRet(i, type::Typed{arg_vals[i], fn_type->output.at(i)}, ctx);
      }

      // Rather than doing this on each block it'd be better to have each
      // scope's destructors jump you to the correct next block for destruction.
      auto *scope = scope_;
      while (scope != nullptr) {
        scope->MakeAllDestructions(ctx);
        if (scope->is<FnScope>()) { break; }
        scope = scope->parent;
      }

      ctx->more_stmts_allowed_ = false;
      ir::ReturnJump();
      return ir::Results{};
    }
    case frontend::Operator::Yield: {
      scope_->MakeAllDestructions(ctx);
      // TODO pretty sure this is all wrong.

      // Can't return these because we need to pass them up at least through the
      // containing statements node and maybe further if we allow labelling
      // scopes to be yielded to.
      ctx->yields_stack_.back().clear();
      ctx->yields_stack_.back().reserve(arg_vals.size());
      // TODO one problem with this setup is that we look things up in a context
      // after returning, so the `after` method has access to a different
      // (smaller) collection of bound constants. This can change the meaning of
      // things or at least make them not compile if the `after` function takes
      // a compile-time constant argument.
      for (size_t i = 0; i < arg_vals.size(); ++i) {
        ctx->yields_stack_.back().emplace_back(args_.exprs_[i].get(),
                                               arg_vals[i]);
      }
      ctx->more_stmts_allowed_ = false;
      return ir::Results{};
    }
    case frontend::Operator::Print: {
      auto const *dispatch_tables = ctx->rep_dispatch_tables(this);
      size_t index                = 0;
      // TODO this is wrong if you use the <<(...) spread operator.
      for (auto &val : arg_vals) {
        auto *t = ctx->type_of(args_.exprs_.at(index).get());
        if (t->is<type::Struct>()) {
          ASSERT_NOT_NULL(dispatch_tables)
              ->at(index)
              .EmitCall(
                  FnArgs<std::pair<Expression *, ir::Results>>(
                      {std::pair(args_.exprs_[index].get(), std::move(val))},
                      {}),
                  type::Void(), ctx);
        } else {
          t->EmitRepr(val, ctx);
        }
        ++index;
      }
      return ir::Results{};
    } break;
    default: UNREACHABLE("Operator is ", static_cast<int>(op_));
  }
}
}  // namespace ast
