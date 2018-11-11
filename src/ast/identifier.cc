#include "ast/identifier.h"

#include "ast/declaration.h"
#include "ast/function_literal.h"
#include "ast/verify_macros.h"
#include "backend/eval.h"
#include "context.h"
#include "error/log.h"
#include "ir/components.h"
#include "ir/val.h"
#include "module.h"
#include "scope.h"
#include "type/pointer.h"
#include "type/type.h"

namespace ast {
void Identifier::assign_scope(Scope *scope) { scope_ = scope; }

type::Type const *Identifier::VerifyType(Context *ctx) {
  type::Type const *t = nullptr;
  if (decl == nullptr) {
    auto[potential_decls, potential_error_decls] =
        scope_->AllDeclsWithId(token, ctx);
    switch (potential_decls.size()) {
      case 1: {
        // TODO could it be that evn though there is only one declaration,
        // there's a bound constant of the same name? If so, we need to deal
        // with this case.
        t    = potential_decls[0].type();
        decl = potential_decls[0].get();
      } break;
      case 0:
        // TODO what if you find a bound constant and some errror decls?
        for (auto const & [ d, v ] :
             ctx->mod_->constants_[ctx->bound_constants_].constants_) {
          if (d->id_ == token) {
            ctx->set_type(this, v.type);
            return v.type;
          }
        }

        switch (potential_error_decls.size()) {
          case 0: ctx->error_log_.UndeclaredIdentifier(this); break;
          case 1: {
            t    = potential_decls[0].type();
            decl = potential_decls[0].get();
            HANDLE_CYCLIC_DEPENDENCIES;
          } break;
          default: NOT_YET();
        }
        limit_to(StageRange::Nothing());
        return nullptr;
      default:
        // TODO Should we allow the overload?
        ctx->error_log_.UnspecifiedOverload(span);
        limit_to(StageRange::Nothing());
        return nullptr;
    }
  }

  if (!decl->const_ && (span.start.line_num < decl->span.start.line_num ||
                        (span.start.line_num == decl->span.start.line_num &&
                         span.start.offset < decl->span.start.offset))) {
    ctx->error_log_.DeclOutOfOrder(decl, this);
    limit_to(StageRange::NoEmitIR());
  }

  // TODO this is because we may have determined the declartaion previously with
  // a different generic setup but not bound the type for this context. But this
  // is wrong in the sense that the declaration bound is possibly dependent on
  // the context.
  if (t == nullptr) { t = ctx->type_of(decl); }

  if (t == nullptr) { return nullptr; }
  ctx->set_type(this, t);
  return t;
}

void Identifier::Validate(Context *ctx) {}

base::vector<ir::Val> ast::Identifier::EmitIR(Context *ctx) {
  if (ASSERT_NOT_NULL(decl)->const_) {
    return decl->EmitIR(ctx);
  } else if (decl->is_arg_) {
    return {ir::Val::Reg(ctx->addr(decl), ctx->type_of(this))};
  } else {
    auto *t = ASSERT_NOT_NULL(ctx->type_of(this));
    return {ir::Val::Reg(ir::PtrFix(EmitLVal(ctx)[0], t), t)};
  }
}

base::vector<ir::Register> ast::Identifier::EmitLVal(Context *ctx) {
  ASSERT(decl != nullptr);
  ASSERT(!decl->const_);
  return {ctx->addr(decl)};
}

}  // namespace ast
