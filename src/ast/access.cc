#include "ast/access.h"

#include "ast/stages.h"
#include "ast/verify_macros.h"
#include "backend/eval.h"
#include "ir/cmd.h"
#include "module.h"
#include "type/array.h"
#include "type/enum.h"
#include "type/function.h"
#include "type/pointer.h"
#include "type/primitive.h"
#include "type/struct.h"
#include "ir/components.h"

namespace AST {
namespace {
using base::check::Is;

const type::Type *DereferenceAll(const type::Type *t) {
  while (t->is<type::Pointer>()) { t = t->as<type::Pointer>().pointee; }
  return t;
}
}  // namespace

void Access::assign_scope(Scope *scope) {
  STAGE_CHECK(AssignScopeStage, AssignScopeStage);
  scope_ = scope;
  operand->assign_scope(scope);
}

type::Type const *Access::VerifyType(Context *ctx) {
  VERIFY_STARTING_CHECK_EXPR;
  VERIFY_OR_RETURN(operand_type, operand);

  auto base_type = DereferenceAll(operand_type);
  if (base_type == type::Type_) {
    auto *evaled_type =
        backend::EvaluateAs<const type::Type *>(operand.get(), ctx);
    if (evaled_type->is<type::Enum>()) {
      // Regardless of whether we can get the value, it's clear that this is
      // supposed to be a member so we should emit an error but carry on
      // assuming that this is an element of that enum type.
      ctx->mod_->types_.buffered_emplace(this, evaled_type);
      type = evaled_type;
      if (evaled_type->as<type::Enum>().IntValueOrFail(member_name) ==
          std::numeric_limits<size_t>::max()) {
        ctx->error_log_.MissingMember(span, member_name, evaled_type);
        limit_to(StageRange::NoEmitIR());
      }
    }
  } else if (base_type->is<type::Struct>()) {
    const auto *member = base_type->as<type::Struct>().field(member_name);
    if (member != nullptr) {
      ctx->mod_->types_.buffered_emplace(this, member->type);
      type = member->type;

    } else {
      ctx->error_log_.MissingMember(span, member_name, base_type);
      type = type::Err;
      limit_to(StageRange::Nothing());
    }
  } else if (base_type == type::Module) {
    type = backend::EvaluateAs<const Module *>(operand.get(), ctx)
          ->GetType(member_name);
    ctx->mod_->types_.buffered_emplace(this, type);
    if (type == nullptr) {
      NOT_YET("log an error");
      type = type::Err;
      limit_to(StageRange::Nothing());
    }

  } else {
    ctx->error_log_.MissingMember(span, member_name, base_type);
    type = type::Err;
    limit_to(StageRange::Nothing());
  }
  return type;
}

void Access::Validate(Context *ctx) {
  STAGE_CHECK(StartBodyValidationStage, DoneBodyValidationStage);
  operand->Validate(ctx);
}

void Access::contextualize(
    const Node *correspondant,
    const base::unordered_map<const Expression *, IR::Val> &replacements){
  operand->contextualize(correspondant->as<Access>().operand.get(),
                         replacements);
}

base::vector<IR::Register> AST::Access::EmitLVal(Context *ctx) {
  auto reg            = operand->EmitLVal(ctx)[0];
  type::Type const *t = type::Ptr(ctx->mod_->types_.at(operand.get()));
  while (!t->as<type::Pointer>().pointee->is_big()) {
    reg = IR::Load(reg, ctx->mod_->types_.at(this));
    t   = t->as<type::Pointer>().pointee;
  }

  auto *struct_type = &t->as<type::Pointer>().pointee->as<type::Struct>();
  return {IR::Field(reg, struct_type, struct_type->index(member_name))};
}

base::vector<IR::Val> AST::Access::EmitIR(Context *ctx) {
  if (ctx->mod_->types_.at(operand.get()) == type::Module) {
    return backend::EvaluateAs<Module const *>(operand.get(), ctx)
        ->GetDecl(member_name)
        ->EmitIR(ctx);
  }

  auto *this_type = ctx->mod_->types_.at(this);
  if (this_type->is<type::Enum>()) {
    return {this_type->as<type::Enum>().EmitLiteral(member_name)};
  } else {
    return {IR::Val::Reg(IR::PtrFix(EmitLVal(ctx)[0], this_type), this_type)};
  }
}

Access *Access::Clone() const {
  auto *result        = new Access;
  result->span        = span;
  result->operand     = base::wrap_unique(operand->Clone());
  result->member_name = member_name;
  return result;
}

}  // namespace AST
