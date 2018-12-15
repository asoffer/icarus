#include "ast/chainop.h"

#include <numeric>

#include "ast/fn_args.h"
#include "ast/overload_set.h"
#include "base/check.h"
#include "context.h"
#include "ir/func.h"
#include "ir/phi.h"
#include "type/array.h"
#include "type/enum.h"
#include "type/flags.h"
#include "type/pointer.h"
#include "type/struct.h"
#include "type/tuple.h"

namespace ir {
Val BlockSeq(base::vector<Val> const &blocks);
RegisterOr<type::Type const *> Variant(base::vector<Val> const &vals);
}  // namespace ir

namespace ast {
namespace {
using base::check::Is;
using base::check::Not;

ir::RegisterOr<bool> EmitChainOpPair(ast::ChainOp *chain_op, size_t index,
                                     ir::Val const &lhs_ir,
                                     ir::Val const &rhs_ir, Context *ctx) {
  auto *lhs_type = ctx->type_of(chain_op->exprs[index].get());
  auto *rhs_type = ctx->type_of(chain_op->exprs[index + 1].get());
  auto op        = chain_op->ops[index];

  if (lhs_type->is<type::Array>() && rhs_type->is<type::Array>()) {
    ASSERT(op == Language::Operator::Eq || op == Language::Operator::Ne);
    return type::Array::Compare(&lhs_type->as<type::Array>(), lhs_ir,
                                &rhs_type->as<type::Array>(), rhs_ir,
                                op == Language::Operator::Eq, ctx)
        .reg_or<bool>();
  } else if (lhs_type->is<type::Struct>() || rhs_type->is<type::Struct>()) {
    FnArgs<std::pair<Expression *, ir::Val>> args;
    args.pos_.reserve(2);
    args.pos_.emplace_back(chain_op->exprs[index].get(), lhs_ir);
    args.pos_.emplace_back(chain_op->exprs[index + 1].get(), rhs_ir);

    auto results =
        chain_op->dispatch_tables_[index].EmitCall(args, type::Bool, ctx);
    ASSERT(results.size() == 1u);
    return results[0].reg_or<bool>();

  } else {
    switch (op) {
      case Language::Operator::Lt:
        return type::ApplyTypes<i8, i16, i32, i64, u8, u16, u32, u64, float,
                                double, ir::FlagsVal>(
            lhs_ir.type, [&](auto type_holder) {
              using T = typename decltype(type_holder)::type;
              return ir::Lt(lhs_ir.reg_or<T>(), rhs_ir.reg_or<T>());
            });
      case Language::Operator::Le:
        return type::ApplyTypes<i8, i16, i32, i64, u8, u16, u32, u64, float,
                                double, ir::FlagsVal>(
            lhs_ir.type, [&](auto type_holder) {
              using T = typename decltype(type_holder)::type;
              return ir::Le(lhs_ir.reg_or<T>(), rhs_ir.reg_or<T>());
            });
      case Language::Operator::Eq: {
        ir::BlockSequence const *val1 =
            std::get_if<ir::BlockSequence>(&lhs_ir.value);
        ir::BlockSequence const *val2 =
            std::get_if<ir::BlockSequence>(&rhs_ir.value);
        if (val1 != nullptr && val2 != nullptr) { return *val1 == *val2; }
      }
        return type::ApplyTypes<bool, i8, i16, i32, i64, u8, u16, u32, u64,
                                float, double, type::Type const *, ir::EnumVal,
                                ir::FlagsVal, ir::Addr>(
            lhs_ir.type, [&](auto type_holder) {
              using T = typename decltype(type_holder)::type;
              return ir::Eq(lhs_ir.reg_or<T>(), rhs_ir.reg_or<T>());
            });
      case Language::Operator::Ne: {
        ir::BlockSequence const *val1 =
            std::get_if<ir::BlockSequence>(&lhs_ir.value);
        ir::BlockSequence const *val2 =
            std::get_if<ir::BlockSequence>(&rhs_ir.value);
        if (val1 != nullptr && val2 != nullptr) { return *val1 == *val2; }
      }
        return type::ApplyTypes<i8, i16, i32, i64, u8, u16, u32, u64, float,
                                double, type::Type const *, ir::EnumVal,
                                ir::FlagsVal, ir::Addr>(
            lhs_ir.type, [&](auto type_holder) {
              using T = typename decltype(type_holder)::type;
              return ir::Ne(lhs_ir.reg_or<T>(), rhs_ir.reg_or<T>());
            });
      case Language::Operator::Ge:
        return type::ApplyTypes<i8, i16, i32, i64, u8, u16, u32, u64, float,
                                double, ir::FlagsVal>(
            lhs_ir.type, [&](auto type_holder) {
              using T = typename decltype(type_holder)::type;
              return ir::Ge(lhs_ir.reg_or<T>(), rhs_ir.reg_or<T>());
            });
      case Language::Operator::Gt:
        return type::ApplyTypes<i8, i16, i32, i64, u8, u16, u32, u64, float,
                                double, ir::FlagsVal>(
            lhs_ir.type, [&](auto type_holder) {
              using T = typename decltype(type_holder)::type;
              return ir::Gt(lhs_ir.reg_or<T>(), rhs_ir.reg_or<T>());
            });
        // TODO case Language::Operator::And: cmp = lhs_ir; break;
      default: UNREACHABLE();
    }
  }
}

}  // namespace

std::string ChainOp::to_string(size_t n) const {
  std::stringstream ss;
  ss << "(";
  for (size_t i = 0; i < ops.size(); ++i) {
    ss << exprs[i]->to_string(n);
    switch (ops[i]) {
      case Language::Operator::Or: ss << " | "; break;
      case Language::Operator::Xor: ss << " ^ "; break;
      case Language::Operator::And: ss << " & "; break;
      case Language::Operator::Lt: ss << " < "; break;
      case Language::Operator::Le: ss << " <= "; break;
      case Language::Operator::Eq: ss << " == "; break;
      case Language::Operator::Ne: ss << " != "; break;
      case Language::Operator::Ge: ss << " >= "; break;
      case Language::Operator::Gt: ss << " > "; break;
      default: UNREACHABLE();
    }
  }
  ss << exprs.back()->to_string(n) << ")";
  return ss.str();
}

void ChainOp::assign_scope(Scope *scope) {
  scope_ = scope;
  for (auto &expr : exprs) { expr->assign_scope(scope); }
}

type::Type const *ChainOp::VerifyType(Context *ctx) {
  std::vector<type::Type const *> expr_types;
  expr_types.reserve(exprs.size());
  for (auto &expr : exprs) { expr_types.push_back(expr->VerifyType(ctx)); }
  if (std::any_of(expr_types.begin(), expr_types.end(),
                  [](type::Type const *t) { return t == nullptr; })) {
    return nullptr;
  }

  if (ops[0] == Language::Operator::Or) {
    bool found_err = false;
    for (size_t i = 0; i < exprs.size() - 1; ++i) {
      if (expr_types[i] == type::Block) {
        ctx->error_log_.EarlyRequiredBlock(exprs[i]->span);
        found_err = true;
      } else if (expr_types[i] == type::OptBlock) {
        continue;
      } else {
        goto not_blocks;
      }
    }
    if (found_err) { return nullptr; }
    type::Type const *last = expr_types.back();
    if (last != type::Block && last != type::OptBlock) {
      goto not_blocks;
    } else {
      return ctx->set_type(this, last);
    }
  }
not_blocks:

  // TODO Can we recover from errors here? Should we?

  // Safe to just check first because to be on the same chain they must all have
  // the same precedence, and ^, &, and | uniquely hold a given precedence.
  switch (ops[0]) {
    case Language::Operator::Or:
    case Language::Operator::And:
    case Language::Operator::Xor: {
      bool failed                       = false;
      type::Type const *first_expr_type = expr_types[0];

      for (auto *expr_type : expr_types) {
        // TODO this collection of error messages could be greatly improved.
        if (expr_type != first_expr_type) {
          auto op_str = [this] {
            switch (ops[0]) {
              case Language::Operator::Or: return "|";
              case Language::Operator::And: return "&";
              case Language::Operator::Xor: return "^";
              default: UNREACHABLE();
            }
          }();
          ctx->error_log_.NoMatchingOperator(op_str, first_expr_type, expr_type,
                                             span);
          failed = true;
        }
      }

      if (failed) { return nullptr; }
      return ctx->set_type(this, first_expr_type);
    } break;
    default: {
      ASSERT(exprs.size() >= 2u);
      for (size_t i = 0; i < exprs.size() - 1; ++i) {
        const type::Type *lhs_type = expr_types[i];
        const type::Type *rhs_type = expr_types[i + 1];

        // TODO struct is wrong. generally user-defined (could be array of
        // struct too, or perhaps a variant containing a struct?) need to
        // figure out the details here.
        const char *token = nullptr;
        switch (ops[i]) {
          case Language::Operator::Lt: token = "<"; break;
          case Language::Operator::Le: token = "<="; break;
          case Language::Operator::Eq: token = "=="; break;
          case Language::Operator::Ne: token = "!="; break;
          case Language::Operator::Ge: token = ">="; break;
          case Language::Operator::Gt: token = ">"; break;
          default: UNREACHABLE();
        }

        if (lhs_type->is<type::Struct>() || rhs_type->is<type::Struct>()) {
          FnArgs<Expression *> args;
          args.pos_ =
              base::vector<Expression *>{{exprs[i].get(), exprs[i + 1].get()}};
          // TODO overwriting type a bunch of times?
          type::Type const *t = nullptr;
          std::tie(dispatch_tables_.at(i), t) =
              DispatchTable::Make(args, OverloadSet(scope_, token, ctx), ctx);
          ASSERT(t, Not(Is<type::Tuple>()));
          if (t == nullptr) { return nullptr; }
        } else {
          if (lhs_type != rhs_type) {
            // TODO better error.
            ctx->error_log_.NoMatchingOperator(token, lhs_type, rhs_type, span);

          } else {
            auto cmp = lhs_type->Comparator();

            switch (ops[i]) {
              case Language::Operator::Eq:
              case Language::Operator::Ne: {
                switch (cmp) {
                  case type::Cmp::Order:
                  case type::Cmp::Equality: continue;
                  case type::Cmp::None:
                    ctx->error_log_.ComparingIncomparables(
                        lhs_type, rhs_type,
                        TextSpan(exprs[i]->span, exprs[i + 1]->span));
                    return nullptr;
                }
              } break;
              case Language::Operator::Lt:
              case Language::Operator::Le:
              case Language::Operator::Ge:
              case Language::Operator::Gt: {
                switch (cmp) {
                  case type::Cmp::Order: continue;
                  case type::Cmp::Equality:
                  case type::Cmp::None:
                    ctx->error_log_.ComparingIncomparables(
                        lhs_type, rhs_type,
                        TextSpan(exprs[i]->span, exprs[i + 1]->span));
                    return nullptr;
                }
              } break;
              default: UNREACHABLE("Expecting a ChainOp operator type.");
            }
          }
        }
      }

      return ctx->set_type(this, type::Bool);
    }
  }
}

void ChainOp::Validate(Context *ctx) {
  for (auto &expr : exprs) { expr->Validate(ctx); }
}

void ChainOp::ExtractJumps(JumpExprs *rets) const {
  for (auto &expr : exprs) { expr->ExtractJumps(rets); }
}

base::vector<ir::Val> ChainOp::EmitIR(Context *ctx) {
  auto *t = ctx->type_of(this);
  if (ops[0] == Language::Operator::Xor) {
    if (t == type::Bool) {
      return {ir::ValFrom(std::accumulate(
          exprs.begin(), exprs.end(), ir::RegisterOr<bool>(false),
          [&](ir::RegisterOr<bool> acc, auto &expr) {
            return ir::XorBool(acc,
                               expr->EmitIR(ctx)[0].template reg_or<bool>());
          }))};
    } else if (t->is<type::Flags>()) {
      return {ir::ValFrom(
          std::accumulate(
              exprs.begin(), exprs.end(),
              ir::RegisterOr<ir::FlagsVal>(ir::FlagsVal{0}),
              [&](ir::RegisterOr<ir::FlagsVal> acc, auto &expr) {
                return ir::XorFlags(
                    &t->as<type::Flags>(), acc,
                    expr->EmitIR(ctx)[0].template reg_or<ir::FlagsVal>());
              }),
          &t->as<type::Flags>())};
    } else {
      UNREACHABLE();
    }

  } else if (ops[0] == Language::Operator::Or && t->is<type::Flags>()) {
    auto iter = exprs.begin();
    auto val  = (*iter)->EmitIR(ctx)[0].reg_or<ir::FlagsVal>();
    while (++iter != exprs.end()) {
      val = ir::OrFlags(&t->as<type::Flags>(), val,
                        (*iter)->EmitIR(ctx)[0].reg_or<ir::FlagsVal>());
    }
    return {ir::ValFrom(val, &t->as<type::Flags>())};
  } else if (ops[0] == Language::Operator::And && t->is<type::Flags>()) {
    auto iter = exprs.begin();
    auto val  = (*iter)->EmitIR(ctx)[0].reg_or<ir::FlagsVal>();
    while (++iter != exprs.end()) {
      val = ir::AndFlags(&t->as<type::Flags>(), val,
                         (*iter)->EmitIR(ctx)[0].reg_or<ir::FlagsVal>());
    }
    return {ir::ValFrom(val, &t->as<type::Flags>())};
  } else if (ops[0] == Language::Operator::Or && t == type::Type_) {
    // TODO probably want to check that each expression is a type? What if I
    // overload | to take my own stuff and have it return a type?
    base::vector<ir::Val> args;
    args.reserve(exprs.size());
    for (const auto &expr : exprs) { args.push_back(expr->EmitIR(ctx)[0]); }
    auto reg_or_type = ir::Variant(args);
    return {ir::ValFrom(reg_or_type)};
  } else if (ops[0] == Language::Operator::Or &&
             (t == type::Block || t == type::OptBlock)) {
    base::vector<ir::Val> vals;
    vals.reserve(exprs.size());
    for (auto &expr : exprs) { vals.push_back(expr->EmitIR(ctx)[0]); }
    return {ir::BlockSeq(vals)};
  } else if (ops[0] == Language::Operator::And ||
             ops[0] == Language::Operator::Or) {
    auto land_block = ir::Func::Current->AddBlock();

    base::unordered_map<ir::BlockIndex, ir::RegisterOr<bool>> phi_args;
    bool is_or = (ops[0] == Language::Operator::Or);
    for (size_t i = 0; i < exprs.size() - 1; ++i) {
      auto val = exprs[i]->EmitIR(ctx)[0].reg_or<bool>();

      auto next_block = ir::Func::Current->AddBlock();
      ir::CondJump(val, is_or ? land_block : next_block,
                   is_or ? next_block : land_block);
      phi_args.emplace(ir::BasicBlock::Current, is_or);

      ir::BasicBlock::Current = next_block;
    }

    phi_args.emplace(ir::BasicBlock::Current,
                     exprs.back()->EmitIR(ctx)[0].reg_or<bool>());
    ir::UncondJump(land_block);

    ir::BasicBlock::Current = land_block;

    return {ir::ValFrom(ir::MakePhi<bool>(ir::Phi(type::Bool), phi_args))};

  } else {
    if (ops.size() == 1) {
      auto lhs_ir = exprs[0]->EmitIR(ctx)[0];
      auto rhs_ir = exprs[1]->EmitIR(ctx)[0];
      return {ir::ValFrom(EmitChainOpPair(this, 0, lhs_ir, rhs_ir, ctx))};

    } else {
      base::unordered_map<ir::BlockIndex, ir::RegisterOr<bool>> phi_args;
      auto lhs_ir     = exprs.front()->EmitIR(ctx)[0];
      auto land_block = ir::Func::Current->AddBlock();
      for (size_t i = 0; i < ops.size() - 1; ++i) {
        auto rhs_ir = exprs[i + 1]->EmitIR(ctx)[0];
        auto cmp    = EmitChainOpPair(this, i, lhs_ir, rhs_ir, ctx);

        phi_args.emplace(ir::BasicBlock::Current, false);
        auto next_block = ir::Func::Current->AddBlock();
        ir::CondJump(cmp, next_block, land_block);
        ir::BasicBlock::Current = next_block;
        lhs_ir                  = rhs_ir;
      }

      // Once more for the last element, but don't do a conditional jump.
      auto rhs_ir = exprs.back()->EmitIR(ctx)[0];
      phi_args.emplace(
          ir::BasicBlock::Current,
          EmitChainOpPair(this, exprs.size() - 2, lhs_ir, rhs_ir, ctx));
      ir::UncondJump(land_block);

      ir::BasicBlock::Current = land_block;

      return {ir::ValFrom(ir::MakePhi<bool>(ir::Phi(type::Bool), phi_args))};
    }
  }
}

base::vector<ir::Register> ChainOp::EmitLVal(Context *ctx) {
  UNREACHABLE(this);
}

}  // namespace ast
