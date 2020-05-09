#include "compiler/dispatch/fn_call_table.h"

#include "compiler/compiler.h"
#include "compiler/dispatch/match.h"
#include "compiler/dispatch/parameters_and_arguments.h"
#include "compiler/dispatch/runtime.h"
#include "core/params_ref.h"
#include "diagnostic/errors.h"
#include "ir/out_params.h"
#include "ir/results.h"
#include "ir/value/generic_fn.h"
#include "type/cast.h"
#include "type/function.h"
#include "type/generic_function.h"
#include "type/type.h"

namespace compiler {
namespace {

std::pair<ir::Results, ir::OutParams> SetReturns(
    ir::Builder &bldr, internal::ExprData const &expr_data) {
  if (auto *fn_type = expr_data.type()->if_as<type::Function>()) {
    auto ret_types = fn_type->output();
    ir::Results results;
    ir::OutParams out_params = bldr.OutParams(ret_types);
    // TODO find a better way to extract these. Figure out why you even need to.
    for (size_t i = 0; i < ret_types.size(); ++i) {
      results.append(out_params[i]);
    }
    return std::pair<ir::Results, ir::OutParams>(std::move(results),
                                                 std::move(out_params));
  } else if (expr_data.type()->is<type::GenericFunction>()) {
    NOT_YET();
  } else {
    NOT_YET(expr_data.type()->to_string());
  }
}

ir::RegOr<ir::Fn> ComputeConcreteFn(Compiler *compiler,
                                    ast::Expression const *fn,
                                    type::Function const *f_type,
                                    type::Quals quals) {
  if (type::Quals::Const() <= quals) {
    return compiler->EmitValue(fn).get<ir::Fn>(0);
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
          compiler->EmitValue(fn).get<ir::Addr>(0));
    }
  }
}

ir::Results EmitCallOneOverload(
    Compiler *compiler, ast::Expression const *fn,
    internal::ExprData const &data,
    core::FnArgs<type::Typed<ir::Value>> const &args) {
  auto callee_qual_type = compiler->qual_type_of(fn);
  ASSERT(callee_qual_type.has_value() == true);

  type::Function const *fn_type = nullptr;
  ir::RegOr<ir::Fn> callee      = [&]() -> ir::RegOr<ir::Fn> {
    if (auto const *gf_type =
            callee_qual_type->type()->if_as<type::GenericFunction>()) {
      fn_type = &data.type()->as<type::Function>();
      ir::GenericFn gen_fn =
          compiler->EmitValue(fn).get<ir::GenericFn>(0).value();
      return ir::Fn(gen_fn.concrete(args));
    } else if (auto const *f_type =
                   callee_qual_type->type()->if_as<type::Function>()) {
      fn_type = f_type;
      return ComputeConcreteFn(compiler, fn, f_type, callee_qual_type->quals());
    } else {
      UNREACHABLE();
    }
  }();

  auto arg_results = args.Transform([](auto const &a) {
    ir::Results res;
    a->apply([&res](auto x) { res.append(x); });
    return type::Typed<ir::Results>(res, a.type());
  });

  if (not callee.is_reg()) {
    switch (callee.value().kind()) {
      case ir::Fn::Kind::Native: {
        core::FillMissingArgs(
            core::ParamsRef(callee.value().native()->params()), &arg_results,
            [compiler](auto const &p) {
              auto results =
                  compiler->EmitValue(ASSERT_NOT_NULL(p.get()->init_val()));
              return type::Typed(results, p.type());
            });
      } break;
      default: break;
    }
  }

  auto[out_results, out_params] = SetReturns(compiler->builder(), data);
  compiler->builder().Call(
      callee, fn_type,
      PrepareCallArguments(compiler, nullptr, data.params(), arg_results),
      out_params);
  return std::move(out_results);
}

type::Typed<ir::Value> ResultsToValue(type::Typed<ir::Results>const& results) {
  ir::Value val(false);
  type::ApplyTypes<bool, int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t,
                   uint32_t, uint64_t, float, double, type::Type const *,
                   ir::EnumVal, ir::FlagsVal, ir::Addr, ir::String, ir::Fn>(
      results.type(), [&](auto tag) -> void {
        using T = typename decltype(tag)::type;
        val     = ir::Value(results->template get<T>(0));
      });
  return type::Typed<ir::Value>(val, results.type());
}

type::Typed<ir::Results> ValueToResults(type::Typed<ir::Value> const &v) {
  ir::Results r;
  v->apply([&](auto x) { r = ir::Results{x}; });
  return type::Typed<ir::Results>(r, v.type());
}

core::FnArgs<type::Typed<ir::Results>> ToResultsArgs(
    core::FnArgs<type::Typed<ir::Value>> const &args) {
  return args.Transform(ValueToResults);
}

core::FnArgs<type::Typed<ir::Value>> ToValueArgs(
    core::FnArgs<type::Typed<ir::Results>> const &args) {
  return args.Transform(ResultsToValue);
}

ir::Results EmitCall(Compiler *compiler,
                     absl::flat_hash_map<ast::Expression const *,
                                         internal::ExprData> const &table,
                     core::FnArgs<type::Typed<ir::Value>> const &args) {
  DEBUG_LOG("FnCallDispatchTable")
  ("Emitting a table with ", table.size(), " entries.");

  if (table.size() == 1) {
    // If there's just one entry in the table we can avoid doing all the work to
    // generate runtime dispatch code. It will amount to only a few
    // unconditional jumps between blocks which will be optimized out, but
    // there's no sense in generating them in the first place..
    auto const & [ overload, expr_data ] = *table.begin();
    return EmitCallOneOverload(compiler, overload, expr_data, args);
  } else {
    auto &bldr           = compiler->builder();
    auto *land_block     = bldr.AddBlock();
    auto callee_to_block = bldr.AddBlocks(table);

    EmitRuntimeDispatch(bldr, table, callee_to_block, ToResultsArgs(args));

    for (auto const & [ overload, expr_data ] : table) {
      bldr.CurrentBlock() = callee_to_block[overload];
      // Argument preparation is done inside EmitCallOneOverload
      EmitCallOneOverload(compiler, overload, expr_data, args);
      // TODO phi-node to coalesce return values.
      bldr.UncondJump(land_block);
    }
    bldr.CurrentBlock() = land_block;
    return ir::Results{};
  }
}

base::expected<absl::flat_hash_map<ast::Expression const *, internal::ExprData>>
Verify(Compiler *compiler, ast::OverloadSet const &os,
       core::FnArgs<type::Typed<ir::Value>> const &args) {
  DEBUG_LOG("dispatch-verify")
  ("Verifying overload set with ", os.members().size(), " members.");

  // Keep a collection of failed matches around so we can give better
  // diagnostics.
  absl::flat_hash_map<ast::Expression const *, FailedMatch> failures;

  auto args_qt = args.Transform(
      [](auto const &t) { return type::QualType::NonConstant(t.type()); });

  absl::flat_hash_map<ast::Expression const *, internal::ExprData> table;
  for (ast::Expression const *overload : os.members()) {
    // TODO the type of the specific overload could *correctly* be null and we
    // need to handle that case.
    DEBUG_LOG("dispatch-verify")
    ("Verifying ", overload, ": ", overload->DebugString());
    if (auto *gen =
            compiler->type_of(overload)->if_as<type::GenericFunction>()) {
      auto val_args                  = args.Transform([](auto const &a) {
        return type::Typed<std::optional<ir::Value>>(a.get(), a.type());
      });
      type::Function const *concrete = gen->concrete(val_args);
      table.emplace(overload,
                    internal::ExprData{concrete, concrete->params(),
                                       concrete->return_types(val_args)});
    } else {
      if (auto result = MatchArgsToParams(ExtractParamTypes(compiler, overload),
                                          args_qt)) {
        // TODO you also call compiler->type_of inside ExtractParamTypess, so
        // it's probably worth reducing the number of lookups.
        table.emplace(overload,
                      internal::ExprData{compiler->type_of(overload), *result});
      } else {
        DEBUG_LOG("dispatch-verify")(result.error());
        failures.emplace(overload, result.error());
      }
    }
  }

  if (not ParamsCoverArgs(args_qt, table,
                          [](auto const &, internal::ExprData const &data) {
                            return data.params();
                          })) {
    // Note: If the overload set is empty, ParamsCoverArgs will emit no
    // diagnostics!
    compiler->diag().Consume(diagnostic::Todo{});
    // TODO Return a failuere-match-reason.
    return base::unexpected("Match failure");
  }

  return table;
}

}  // namespace

ir::Results FnCallDispatchTable::Emit(
    Compiler *c, ast::OverloadSet const &os,
    core::FnArgs<type::Typed<ir::Results>> const &r_args) {
  auto args = ToValueArgs(r_args);
  ASSIGN_OR(return ir::Results{},  //
                   auto table, Verify(c, os, args));
  return EmitCall(c, table, args);
}

type::QualType FnCallDispatchTable::ComputeResultQualType(
    absl::Span<type::Function const *const> &fn_types) {
  std::vector<absl::Span<type::Type const *const>> results;
  for (auto const *fn_type : fn_types) { results.push_back(fn_type->output()); }
  return type::QualType(type::MultiVar(results), type::Quals::Unqualified());
}

type::QualType FnCallDispatchTable::ComputeResultQualType(
    absl::flat_hash_map<ast::Expression const *, internal::ExprData> const
        &table) {
  std::vector<absl::Span<type::Type const *const>> results;
  for (auto const & [ overload, expr_data ] : table) {
    DEBUG_LOG("dispatch-verify")
    ("Extracting return type for ", overload->DebugString(), " of type ",
     expr_data.type()->to_string());
    if (auto *fn_type = expr_data.type()->if_as<type::Function>()) {
      auto out_span = fn_type->output();
      results.push_back(out_span);
    } else if (expr_data.type()->is<type::GenericFunction>()) {
      results.emplace_back();  // NOT_YET figuring out the real answer.
    } else {
      NOT_YET(expr_data.type()->to_string());
    }
  }

  return type::QualType(type::MultiVar(results), type::Quals::Unqualified());
}

}  // namespace compiler
