#ifndef ICARUS_COMPILER_WORK_ITEM_H
#define ICARUS_COMPILER_WORK_ITEM_H

#include "base/extend.h"
#include "base/extend/absl_hash.h"
#include "base/meta.h"

namespace compiler {
struct Context;

// A `WorkItem` is a description of a task to be completed by the compiler
// infrastructure. Each `WorkItem` consists of an enumerator describing the kind
// of work to be done, along with the AST node on which the work should be done.
struct WorkItem : base::Extend<WorkItem>::With<base::AbslHashExtension> {
  enum class Kind {
    VerifyType,
    VerifyEnumBody,
    VerifyFunctionBody,
    VerifyStructBody,
    CompleteStructMembers,
    EmitFunctionBody,
    EmitShortFunctionBody,
    EmitJumpBody,
  };

  static WorkItem VerifyBodyOf(auto const *n, Context *ctx) {
    constexpr auto type = base::meta<std::decay_t<decltype(*n)>>;
    if constexpr (type == base::meta<ast::EnumLiteral>) {
      return {.kind = Kind::VerifyEnumBody, .node = n, .context = ctx};
    } else if constexpr (type == base::meta<ast::FunctionLiteral>) {
      return {.kind = Kind::VerifyFunctionBody, .node = n, .context = ctx};
    } else if constexpr (type == base::meta<ast::StructLiteral>) {
      return {.kind = Kind::VerifyStructBody, .node = n, .context = ctx};
    } else {
      static_assert(base::always_false(type));
    }
  }

  Kind kind;
  ast::Node const *node;
  Context *context;
};

}  // namespace compiler

#endif  // ICARUS_COMPILER_WORK_ITEM_H