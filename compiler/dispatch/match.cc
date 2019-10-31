#include "compiler/dispatch/match.h"

#include "ast/ast.h"
#include "type/cast.h"
#include "type/type.h"

namespace compiler {
namespace {

std::optional<FailedMatch> MatchPositionalArgsToParams(
    core::FnParams<type::Typed<ast::Declaration const *>> const &params,
    core::FnArgs<compiler::VerifyResult> const &args,
    core::FnParams<type::Type const *> *matched_params) {
  for (size_t i = 0; i < args.pos().size(); ++i) {
    auto const &param = params.at(i);
    type::Type const *meet =
        type::Meet(args.at(i).type(), params.at(i).value.type());
    if (not meet) { return FailedMatch{}; }
    // TODO understand why copying flags is wrong here and explain it.
    matched_params->append(param.name, meet);
  }
  return std::nullopt;
}

std::optional<FailedMatch> MatchNamedArgsToParams(
    core::FnParams<type::Typed<ast::Declaration const *>> const &params,
    core::FnArgs<compiler::VerifyResult> const &args,
    core::FnParams<type::Type const *> *matched_params) {
  size_t named_start_index = args.pos().size();
  for (size_t i = named_start_index; i < params.size(); ++i) {
    auto const &param = params.at(i);
    auto *result      = args.at_or_null(param.name);
    if (not result) {
      // No argument provided by that name? This could be because we have
      // default parameters or an empty variadic pack.
      // TODO: Handle variadic packs.
      if (param.flags & core::HAS_DEFAULT) {
      } else {
        return FailedMatch{};
      }
    } else {
      if ((*param.value)->flags() & ast::Declaration::f_IsConst) {
        NOT_YET();
      } else {
        type::Type const *meet = type::Meet(result->type(), param.value.type());
        if (not meet) { return FailedMatch{}; }
        matched_params->append(param.name, meet);
      }
    }
  }
  return std::nullopt;
}

}  // namespace

base::expected<core::FnParams<type::Type const *>, FailedMatch>
MatchArgsToParams(
    core::FnParams<type::Typed<ast::Declaration const *>> const &params,
    core::FnArgs<compiler::VerifyResult> const &args) {
  if (args.size() > params.size()) { FailedMatch{}; }

  core::FnParams<type::Type const *> matched_params;
  if (auto failure =
          MatchPositionalArgsToParams(params, args, &matched_params)) {
    return *failure;
  }
  if (auto failure = MatchNamedArgsToParams(params, args, &matched_params)) {
    return *failure;
  }
  return matched_params;
}

}  // namespace compiler
