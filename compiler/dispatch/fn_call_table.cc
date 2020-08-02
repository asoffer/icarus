#include "compiler/dispatch/fn_call_table.h"

#include "base/meta.h"
#include "compiler/compiler.h"
#include "compiler/dispatch/match.h"
#include "compiler/dispatch/parameters_and_arguments.h"
#include "compiler/dispatch/runtime.h"
#include "core/params_ref.h"
#include "diagnostic/errors.h"
#include "ir/out_params.h"
#include "ir/value/generic_fn.h"
#include "ir/value/value.h"
#include "type/cast.h"
#include "type/function.h"
#include "type/generic_function.h"
#include "type/type.h"

namespace compiler {
namespace {

struct MoveInitTag {};
struct CopyInitTag {};
struct AssignTag {};

template<typename Tag>
ir::OutParams SetReturns(Tag,
    ir::Builder &bldr, type::Type const *type,
    absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to) {
  if (auto *fn_type = type->if_as<type::Function>()) {
    if constexpr (base::meta<Tag> == base::meta<MoveInitTag>) {
      return bldr.OutParamsMoveInit(fn_type->output(), to);
    } else if constexpr (base::meta<Tag> == base::meta<CopyInitTag>) {
      return bldr.OutParamsCopyInit(fn_type->output(), to);
    } else if constexpr (base::meta<Tag> == base::meta<AssignTag>) {
      return bldr.OutParamsAssign(fn_type->output(), to);
    } else {
      static_assert(base::always_false<Tag>());
    }
  } else if (type->is<type::GenericFunction>()) {
    NOT_YET(type->to_string());
  } else {
    NOT_YET(type->to_string());
  }
}

ir::RegOr<ir::Fn> ComputeConcreteFn(Compiler *compiler,
                                    ast::Expression const *fn,
                                    type::Function const *f_type,
                                    type::Quals quals) {
  if (type::Quals::Const() <= quals) {
    return compiler->EmitValue(fn).get<ir::RegOr<ir::Fn>>();
  } else {
    // NOTE: If the overload is a declaration, it's not because a
    // declaration is syntactically the callee. Rather, it's because the
    // callee is an identifier (or module_name.identifier, etc.) and this
    // is one possible resolution of that identifier. We cannot directly
    // ask to emit IR for the declaration because that will emit the
    // initialization for the declaration. Instead, we need load the
    // address.
    if (auto *fn_decl = fn->if_as<ast::Declaration>()) {
      return compiler->builder().Load<ir::Fn>(compiler->data().addr(fn_decl));
    } else {
      return compiler->builder().Load<ir::Fn>(
          compiler->EmitValue(fn).get<ir::RegOr<ir::Addr>>());
    }
  }
}

std::tuple<ir::RegOr<ir::Fn>, type::Function const *, DependentComputedData *>
EmitCallee(Compiler &compiler, ast::Expression const *fn, type::QualType qt,
           const core::FnArgs<type::Typed<ir::Value>> &args) {
  if (auto const *gf_type = qt.type()->if_as<type::GenericFunction>()) {
    ir::GenericFn gen_fn =
        compiler.EmitValue(fn).get<ir::RegOr<ir::GenericFn>>().value();

    // TODO declarations aren't callable so we shouldn't have to check this
    // here.

    if (auto const *decl = fn->if_as<ast::Declaration>()) {
      // TODO make this more robust.
      fn = decl->init_val();
    }

    auto *parameterized_expr = &fn->as<ast::ParameterizedExpression>();

    DependentComputedData temp_data(&compiler.data().module());
    Compiler c({
        .builder             = compiler.builder(),
        .data                = temp_data,
        .diagnostic_consumer = compiler.diag(),
    });
    temp_data.parent_ = &compiler.data();

    auto params = c.ComputeParamsFromArgs(
        parameterized_expr, OrderedDependencyNodes(parameterized_expr), args);

    auto find_dependent_result = compiler.data().FindDependent(
        &fn->as<ast::ParameterizedExpression>(), params);
    return std::make_tuple(ir::Fn(gen_fn.concrete(args)),
                           find_dependent_result.fn_type,
                           &find_dependent_result.data);
  } else if (auto const *f_type = qt.type()->if_as<type::Function>()) {
    return std::make_tuple(ComputeConcreteFn(&compiler, fn, f_type, qt.quals()),
                           f_type, nullptr);
  } else {
    UNREACHABLE();
  }
}

template <typename Tag>
void EmitCallOneOverload(
    Tag, Compiler *compiler, ast::Expression const *fn,
    core::FnArgs<type::Typed<ir::Value>> args,
    absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to) {
  auto callee_qual_type = compiler->qual_type_of(fn);
  ASSERT(callee_qual_type.has_value() == true);

  auto [callee, fn_type, dependent_data] =
      EmitCallee(*compiler, fn, *callee_qual_type, args);

  Compiler c({
      .builder = ir::GetBuilder(),
      .data    = dependent_data ? *dependent_data : compiler->data(),
      .diagnostic_consumer = compiler->diag(),
  });

  if (not callee.is_reg()) {
    switch (callee.value().kind()) {
      case ir::Fn::Kind::Native: {
        core::FillMissingArgs(
            core::ParamsRef(callee.value().native()->params()), &args,
            [&c](auto const &p) {
              return type::Typed(
                  c.EmitValue(ASSERT_NOT_NULL(p.get()->init_val())), p.type());
            });
      } break;
      default: break;
    }
  }

  auto out_params = SetReturns(Tag{}, c.builder(), fn_type, to);
  c.builder().Call(callee, fn_type,
                   PrepareCallArguments(&c, nullptr, fn_type->params(), args),
                   out_params);
  int i = -1;
  for (auto const *t : fn_type->output()) {
    ++i;
    if (t->is_big()) continue;
    c.Visit(t, *to[i], type::Typed{ir::Value(out_params[i]), t},
            EmitCopyAssignTag{});
  }
}

template <typename Tag>
void EmitCall(Tag, Compiler *compiler, ast::OverloadSet const &os,
              core::FnArgs<type::Typed<ir::Value>> const &args,
              absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to) {
  DEBUG_LOG("FnCallDispatchTable")
  ("Emitting a table with ", os.members().size(), " entries.");

  if (os.members().size() == 1) {
    // If there's just one entry in the table we can avoid doing all the work to
    // generate runtime dispatch code. It will amount to only a few
    // unconditional jumps between blocks which will be optimized out, but
    // there's no sense in generating them in the first place..
    auto const *overload = *os.members().begin();
    EmitCallOneOverload(CopyInitTag{}, compiler, overload, args, to);
  } else {
    auto &bldr           = compiler->builder();
    auto *land_block     = bldr.AddBlock();
    auto callee_to_block = bldr.AddBlocks(os);

    EmitRuntimeDispatch(compiler->data(), compiler->builder(), os,
                        callee_to_block, args);

    for (auto const *overload : os.members()) {
      bldr.CurrentBlock() = callee_to_block[overload];
      // Argument preparation is done inside EmitCallOneOverload
      EmitCallOneOverload(CopyInitTag{}, compiler, overload, args, to);
      // TODO phi-node to coalesce return values.
      bldr.UncondJump(land_block);
    }
    bldr.CurrentBlock() = land_block;
  }
}

}  // namespace

void FnCallDispatchTable::EmitCopyInit(
    Compiler *c, ast::OverloadSet const &os,
    core::FnArgs<type::Typed<ir::Value>> const &args,
    absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to) {
  return EmitCall(CopyInitTag{}, c, os, args, to);
}

void FnCallDispatchTable::EmitMoveInit(
    Compiler *c, ast::OverloadSet const &os,
    core::FnArgs<type::Typed<ir::Value>> const &args,
    absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to) {
  return EmitCall(MoveInitTag{}, c, os, args, to);
}

void FnCallDispatchTable::EmitAssign(
    Compiler *c, ast::OverloadSet const &os,
    core::FnArgs<type::Typed<ir::Value>> const &args,
    absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to) {
  return EmitCall(AssignTag{}, c, os, args, to);
}

}  // namespace compiler
