#ifndef ICARUS_SEMANTIC_ANALYSIS_TYPE_VERIFICATION_VERIFY_H
#define ICARUS_SEMANTIC_ANALYSIS_TYPE_VERIFICATION_VERIFY_H

#include <span>

#include "ast/ast.h"
#include "ast/module.h"
#include "diagnostic/consumer/consumer.h"
#include "jasmin/execute.h"
#include "module/module.h"
#include "module/module_map.h"
#include "semantic_analysis/byte_code/byte_code.h"
#include "semantic_analysis/context.h"
#include "semantic_analysis/instruction_set.h"
#include "semantic_analysis/task.h"
#include "semantic_analysis/type_system.h"
#include "semantic_analysis/type_verification/diagnostics.h"

namespace semantic_analysis {

enum class TypeVerificationPhase {
  VerifyParameters,
  VerifyType,
  VerifyBody,
  Completed,
};

namespace internal_verify {

using Types =
    std::tuple<std::span<absl::flat_hash_map<
                   core::ParameterType, Context::CallableIdentifier> const>,
               std::span<QualifiedType const>, void, void>;
template <TypeVerificationPhase P>
using ReturnType = std::tuple_element_t<static_cast<int>(P), Types>;

}  // namespace internal_verify

using VerificationTask =
    Task<ast::Node const *, TypeVerificationPhase, internal_verify::ReturnType>;
using VerificationScheduler = Scheduler<VerificationTask>;

inline auto VerifyTypeOf(ast::Node const *node) {
  return VerificationTask::Phase<TypeVerificationPhase::VerifyType>(node);
}

inline auto VerifyParametersOf(ast::Node const *node) {
  return VerificationTask::Phase<TypeVerificationPhase::VerifyParameters>(node);
}

struct TypeVerifier : VerificationScheduler {
  using signature = VerificationTask();

  explicit TypeVerifier(module::ModuleMap &module_map, Context &c,
                        diagnostic::DiagnosticConsumer &d)
      : VerificationScheduler([](VerificationScheduler &s,
                                 ast::Node const *node) -> VerificationTask {
          return node->visit(static_cast<TypeVerifier &>(s));
        }),
        module_map_(module_map),
        module_(module_map_.primary()),
        context_(c),
        diagnostic_consumer_(d) {}

  Context &context() const { return context_; }
  TypeSystem &type_system() const { return module_.type_system(); }
  ForeignFunctionMap &foreign_function_map() const {
    return module_.foreign_function_map();
  }
  auto &module() { return module_; }
  auto const &module() const { return module_; }
  auto &module_map() { return module_map_; }
  auto const &module_map() const { return module_map_; }

  template <typename T>
  T EvaluateAs(ast::Expression const *expression) const {
    return ::semantic_analysis::EvaluateAs<T>(context(), module_, expression);
  }

  std::span<std::byte const> EvaluateConstant(ast::Expression const *expr,
                                              QualifiedType qt) {
    return ::semantic_analysis::EvaluateConstant(context(), module_, expr, qt);
  }

  template <typename D>
  void ConsumeDiagnostic(D &&d) {
    diagnostic_consumer_.Consume(std::forward<D>(d));
  }

  VerificationTask operator()(auto const *node) {
    return VerifyType(*this, node);
  }

  auto TypeOf(ast::Expression const *node, QualifiedType qualified_type) {
    return VerificationTask::YieldResult<TypeVerificationPhase::VerifyType>(
        node, context().set_qualified_type(node, qualified_type));
  }
  auto TypeOf(ast::Expression const *node,
              std::vector<QualifiedType> qualified_types) {
    return VerificationTask::YieldResult<TypeVerificationPhase::VerifyType>(
        node, context().set_qualified_types(node, std::move(qualified_types)));
  }
  auto TypeOf(ast::Expression const *node,
              std::span<QualifiedType const> qualified_types) {
    return VerificationTask::YieldResult<TypeVerificationPhase::VerifyType>(
        node,
        context().set_qualified_types(
            node, std::vector(qualified_types.begin(), qualified_types.end())));
  }

  auto ParametersOf(
      ast::Expression const *node,
      std::vector<
          absl::flat_hash_map<core::ParameterType, Context::CallableIdentifier>>
          parameter_ids) {
    return VerificationTask::YieldResult<
        TypeVerificationPhase::VerifyParameters>(
        node, context().set_parameters(node, std::move(parameter_ids)));
  }

  auto ParametersOf(
      ast::Expression const *node,
      absl::flat_hash_map<core::ParameterType, Context::CallableIdentifier>
          parameter_ids) {
    std::vector<decltype(parameter_ids)> v;
    v.push_back(std::move(parameter_ids));
    return ParametersOf(node, std::move(v));
  }

  auto Completed(ast::Node const *node) {
    return VerificationTask::YieldResult<TypeVerificationPhase::Completed>(
        node);
  }

  template <typename NodeType>
  static VerificationTask VerifyType(TypeVerifier &tv, NodeType const *) {
    NOT_YET(nth::type<NodeType>);
  }

  static VerificationTask VerifyType(TypeVerifier &, ast::Access const *);
  static VerificationTask VerifyType(TypeVerifier &, ast::Assignment const *);
  static VerificationTask VerifyType(TypeVerifier &, ast::ArrayLiteral const *);
  static VerificationTask VerifyType(TypeVerifier &, ast::ArrayType const *);
  static VerificationTask VerifyType(TypeVerifier &,
                                     ast::BinaryOperator const *);
  static VerificationTask VerifyType(TypeVerifier &,
                                     ast::BindingDeclaration const *);
  static VerificationTask VerifyType(TypeVerifier &, ast::Builtin const *);
  static VerificationTask VerifyType(TypeVerifier &, ast::Call const *);
  static VerificationTask VerifyType(TypeVerifier &, ast::Cast const *);
  static VerificationTask VerifyType(TypeVerifier &,
                                     ast::ComparisonOperator const *);
  static VerificationTask VerifyType(TypeVerifier &, ast::Declaration const *);
  static VerificationTask VerifyType(TypeVerifier &,
                                     ast::Declaration::Id const *);
  static VerificationTask VerifyType(TypeVerifier &,
                                     ast::FunctionLiteral const *);
  static VerificationTask VerifyType(TypeVerifier &, ast::FunctionType const *);
  static VerificationTask VerifyType(TypeVerifier &, ast::Identifier const *);
  static VerificationTask VerifyType(TypeVerifier &, ast::IfStmt const *);
  static VerificationTask VerifyType(TypeVerifier &, ast::Import const *);
  static VerificationTask VerifyType(TypeVerifier &, ast::Module const *);
  static VerificationTask VerifyType(TypeVerifier &,
                                     ast::ShortFunctionLiteral const *);
  static VerificationTask VerifyType(TypeVerifier &, ast::SliceType const *);
  static VerificationTask VerifyType(TypeVerifier &,
                                     ast::ProgramArguments const *);
  static VerificationTask VerifyType(TypeVerifier &, ast::ReturnStmt const *);
  static VerificationTask VerifyType(TypeVerifier &,
                                     ast::UnaryOperator const *);
  static VerificationTask VerifyType(TypeVerifier &, ast::Terminal const *);

  // TODO: ArgumentType, BinaryAssignmentOperator, BlockNode,
  //       DesignatedInitializer, EnumLiteral, Index, InterfaceLiteral,
  //       Label, ScopeLiteral, ScopeNode, StructLiteral, YieldStmt,
  //       WhileStmt.

  std::string TypeForDiagnostic(ast::Expression const &expression) const {
    return ::semantic_analysis::TypeForDiagnostic(expression, context(),
                                                  type_system());
  }

 private:
  module::ModuleMap &module_map_;
  module::Module &module_;
  Context &context_;
  diagnostic::DiagnosticConsumer &diagnostic_consumer_;
};

}  // namespace semantic_analysis

#endif  // ICARUS_SEMANTIC_ANALYSIS_TYPE_VERIFICATION_VERIFY_H
