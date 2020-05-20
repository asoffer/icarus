#include "ast/ast.h"
#include "compiler/compiler.h"
#include "diagnostic/errors.h"
#include "diagnostic/message.h"
#include "type/enum.h"
#include "type/flags.h"
#include "type/overload_set.h"
#include "type/primitive.h"
#include "type/qual_type.h"
#include "type/struct.h"

#include <utility>
#include <string>

namespace compiler {
namespace {

struct MissingMember {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "missing-member";

  diagnostic::DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text("Expressions of type `%s` have no member named `%s`.",
                         type->to_string(), member),
        diagnostic::SourceQuote(src).Highlighted(range, diagnostic::Style{}));
  }
  frontend::SourceRange range;
  std::string member;
  type::Type const *type;
};

struct NonConstantTypeMemberAccess {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName = "non-constant-type-member-access";

  diagnostic::DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text("Cannot access a member of a non-constant type."),
        diagnostic::SourceQuote(src).Highlighted(range, diagnostic::Style{}));
  }

  frontend::SourceRange range;
};

struct TypeHasNoMembers {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "type-has-no-members";

  diagnostic::DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text("Cannot access a member of `type`."),
        diagnostic::SourceQuote(src).Highlighted(range, diagnostic::Style{}));
  }

  frontend::SourceRange range;
};

struct NonExportedMember {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "non-exported-member";

  diagnostic::DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text(
            "Expressions of type `%s` do not export the member `%s`.",
            type->to_string(), member),
        diagnostic::SourceQuote(src).Highlighted(range, diagnostic::Style{}));
  }

  std::string member;
  type::Type const *type;
  frontend::SourceRange range;
};

struct NonConstantModuleMemberAccess {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName = "non-constant-module-member-access";

  diagnostic::DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text("Cannot access a member of a non-constant module."),
        diagnostic::SourceQuote(src).Highlighted(range, diagnostic::Style{}));
  }

  frontend::SourceRange range;
};

struct UndeclaredIdentifierInModule {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName = "undeclared-identifier-in-module";

  diagnostic::DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text("Module contains no exported member `%s`", id),
        diagnostic::SourceQuote(src).Highlighted(
            range, diagnostic::Style::ErrorText()));
  }

  std::string_view id;
  frontend::SourceRange range;
};

// Returns a pair whose first element is the type obtained by dereferencing the
// argument as many times as possible, and whose second element is the number of
// dereferences requried.
std::pair<type::Type const *, int> DereferenceAll(type::Type const *t) {
  int num_derefs = 0;
  while (auto *p = t->if_as<type::Pointer>()) {
    t = p->pointee();
    ++num_derefs;
  }
  return std::pair(t, num_derefs);
}

// Verification of an `Access` expression when the operand is a type. This is
// only allowed when the type in question is constant and is an enum or flags.
type::QualType AccessTypeMember(Compiler *c, ast::Access const *node,
                                type::QualType operand_qt) {
  if (not operand_qt.constant()) {
    c->diag().Consume(NonConstantTypeMemberAccess{
        .range = node->range(),
    });
    return type::QualType::Error();
  }

  auto maybe_type = c->EvaluateAs<type::Type const *>(node->operand());
  if (not maybe_type) {
    c->diag().Consume(diagnostic::EvaluationFailure{
        .failure = maybe_type.error(),
        .range   = node->range(),
    });
    return type::QualType::Error();
  }

  auto *evaled_type = *maybe_type;
  auto qt           = type::QualType::Constant(evaled_type);

  // For enums and flags, regardless of whether we can get the value, it's
  // clear that node is supposed to be a member so we should emit an error but
  // carry on assuming that node is an element of that enum type.
  if (auto *e = evaled_type->if_as<type::Enum>()) {
    if (not e->Get(node->member_name()).has_value()) {
      c->diag().Consume(MissingMember{
          .range  = node->range(),
          .member = std::string{node->member_name()},
          .type   = evaled_type,
      });

      // We can continue passed this error because we are confident that this is
      // an enumerator, but we should not emit code for it.
      qt.MarkError();
    }
    return c->data().set_qual_type(node, qt);
  }

  if (auto *f = evaled_type->if_as<type::Flags>()) {
    if (not f->Get(node->member_name()).has_value()) {
      c->diag().Consume(MissingMember{
          .range  = node->range(),
          .member = std::string{node->member_name()},
          .type   = evaled_type,
      });

      // We can continue passed this error because we are confident that this is
      // a flag, but we should not emit code for it.
      qt.MarkError();
    }
    return c->data().set_qual_type(node, qt);
  }

  // TODO: Determine whether structs are allowed to have constant members
  // accessible through the type-name. At the moment this is not allowed.
  c->diag().Consume(TypeHasNoMembers{.range = node->range()});
  return type::QualType::Error();
}

// Verifies access to a struct field. The field must be exported if it is in a
// different module.
type::QualType AccessStructMember(Compiler *c, ast::Access const *node,
                                  type::Struct const *s, type::Quals quals) {
  auto const *member = s->field(node->member_name());
  if (member == nullptr) {
    c->diag().Consume(MissingMember{
        .range  = node->range(),
        .member = std::string{node->member_name()},
        .type   = s,
    });
    return type::QualType::Error();
  }

  type::QualType qt(member->type, quals | type::Quals::Ref());

  // Struct field members need to be exported in addition to the struct itself.
  if (c->data().module() != s->defining_module() and
      not member->contains_hashtag(
          ast::Hashtag(ast::Hashtag::Builtin::Export))) {
    c->diag().Consume(NonExportedMember{
        .member = std::string{node->member_name()},
        .type   = s,
        .range  = node->range(),
    });

    // We can continue passed this error because we are confident about the
    // fields type, but we should not emit code for it.
    qt.MarkError();
  }

  return c->data().set_qual_type(node, qt);
}

// Verifies access to a symbol in a different module. If there are multiple
// symbols of the same name, verify that they form a valid overload set.
type::QualType AccessModuleMember(Compiler *c, ast::Access const *node,
                                  type::QualType operand_qt) {
  if (not operand_qt.constant()) {
    c->diag().Consume(NonConstantModuleMemberAccess{
        .range = node->range(),
    });
    return type::QualType::Error();
  }

  auto maybe_mod = c->EvaluateAs<module::BasicModule *>(node->operand());
  if (not maybe_mod) {
    c->diag().Consume(diagnostic::EvaluationFailure{
        .failure = maybe_mod.error(),
        .range   = node->range(),
    });
    return type::QualType::Error();
  }
  // TODO: Rather than evaluating as a BasicModule and then down-casting to
  // CompiledModule, we should make this cast unnecessary.
  auto const *mod = &(*maybe_mod)->as<CompiledModule>();

  // There is no way to refer to the current module, but a bug here could cause
  // a deadlock as this module waits for the notification that it's declarations
  // can be exported, so we would prefer to abort.
  ASSERT(mod != c->data().module());

  auto decls = mod->ExportedDeclarations(node->member_name());
  switch (decls.size()) {
    case 0: {
      c->diag().Consume(UndeclaredIdentifierInModule{
          .id    = node->member_name(),
          .range = node->range(),
      });
      return type::QualType::Error();
    } break;
    case 1: {
      auto const *qt = mod->data().qual_type(decls[0]);

      type::Type const *t = mod->data().qual_type(decls[0])->type();
      if (qt == nullptr or not qt->ok()) {
        DEBUG_LOG("AccessModuleMember")
        ("Found member in a different module that is missing a type. Suspected "
         "error generated from that module: ",
         node->DebugString());
        return type::QualType::Error();
      } else {
        return c->data().set_qual_type(node, *qt);
      }

    } break;
    default: {
      // TODO: these may also be an overload set of scopes
      type::Quals quals = type::Quals::Const();
      absl::flat_hash_set<type::Callable const *> member_types;
      auto const &data = mod->data();
      for (auto const *decl : decls) {
        auto *qt = data.qual_type(decl);
        if (qt == nullptr or not qt->ok()) {
          DEBUG_LOG("AccessModuleMember")
          ("Found member in a different module that is missing a type. "
           "Suspected error generated from that module: ",
           decl->DebugString());
          return type::QualType::Error();
        }

        if (auto *callable = qt->type()->if_as<type::Callable>()) {
          quals &= qt->quals();
          member_types.insert(callable);
        } else {
          DEBUG_LOG("AccessModuleMember")
          ("Non-callable found in an overload set across module boundaries. An "
           "error should have already been generated by that module: ",
           decl->DebugString());
          return type::QualType::Error();
        }
      }

      return c->data().set_qual_type(
          node, type::QualType(type::MakeOverloadSet(std::move(member_types)),
                               quals));
    } break;
  }
  UNREACHABLE();
}

}  // namespace

type::QualType Compiler::VerifyType(ast::Access const *node) {
  ASSIGN_OR(return type::QualType::Error(),  //
                   auto operand_result, VerifyType(node->operand()));

  auto [base_type, num_derefs] = DereferenceAll(operand_result.type());
  if (base_type == type::Type_) {
    return AccessTypeMember(this, node, operand_result);
  } else if (base_type == type::Module) {
    return AccessModuleMember(this, node, operand_result);
  } else {
    auto quals = operand_result.quals();
    if (num_derefs > 0) { quals |= type::Quals::Ref(); }

    if (base_type == type::ByteView) {
      if (node->member_name() == "length") {
        return data().set_qual_type(
            node, type::QualType(type::Int64, quals | type::Quals::Const()));
      } else {
        diag().Consume(MissingMember{
            .range  = node->range(),
            .member = std::string{node->member_name()},
            .type   = type::ByteView,
        });
        return type::QualType::Error();
      }
    } else if (auto *s = base_type->if_as<type::Struct>()) {
      return AccessStructMember(this, node, s, quals);
    } else {
      // TODO: Improve this error message. It's not just that the member is
      // missing but that we don't even think it's allowed to have a member on a
      // value of this type.
      diag().Consume(MissingMember{
          .range  = node->range(),
          .member = std::string{node->member_name()},
          .type   = base_type,
      });
      return type::QualType::Error();
    }
  }
}

}  // namespace compiler