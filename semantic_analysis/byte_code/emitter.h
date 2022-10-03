#ifndef ICARUS_SEMANTIC_ANALYSIS_BYTE_CODE_EMITTER_H
#define ICARUS_SEMANTIC_ANALYSIS_BYTE_CODE_EMITTER_H

#include "ast/expression.h"
#include "jasmin/execute.h"
#include "jasmin/function.h"
#include "jasmin/instructions/core.h"
#include "semantic_analysis/byte_code/instruction_set.h"
#include "semantic_analysis/context.h"
#include "semantic_analysis/type_system.h"

namespace semantic_analysis {

struct ByteCodeEmitterBase {
  using signature = void(IrFunction &);

  explicit ByteCodeEmitterBase(Context const *c,
                               ForeignFunctionMap &foreign_function_map,
                               TypeSystem &type_system)
      : foreign_function_map_(foreign_function_map),
        type_system_(type_system),
        context_(ASSERT_NOT_NULL(c)) {}

  Context const &context() const { return *context_; }

  auto &type_system() const { return type_system_; }
  auto &foreign_function_map() const { return foreign_function_map_; }

 private:
  ForeignFunctionMap &foreign_function_map_;
  TypeSystem &type_system_;
  Context const *context_;
};

struct ByteCodeValueEmitter : ByteCodeEmitterBase {
  explicit ByteCodeValueEmitter(Context const *c,
                                ForeignFunctionMap &foreign_function_map,
                                TypeSystem &type_system)
      : ByteCodeEmitterBase(c, foreign_function_map, type_system) {}

  void operator()(auto const *node, IrFunction &f) { return Emit(node, f); }

  void EmitByteCode(ast::Node const *node, IrFunction &f) {
    node->visit<ByteCodeValueEmitter>(*this, f);
  }

  template <typename T>
  std::optional<T> EvaluateAs(ast::Expression const *expression) {
    auto qt        = context().qualified_type(expression);
    bool has_error = (qt.qualifiers() >= Qualifiers::Error());
    ASSERT(has_error == false);

    IrFunction f(0, 1);
    EmitByteCode(expression, f);
    f.append<jasmin::Return>();

    T result;
    jasmin::Execute(f, {}, result);
    return result;
  }

  template <typename NodeType>
  void Emit(NodeType const *node, IrFunction &f) {
    NOT_YET(base::meta<NodeType>);
  }

  void Emit(ast::Call const *node, IrFunction &f);
  void Emit(ast::FunctionType const *node, IrFunction &f);
  void Emit(ast::UnaryOperator const *node, IrFunction &f);
  void Emit(ast::Terminal const *node, IrFunction &f);
};

}  // namespace semantic_analysis

#endif  // ICARUS_SEMANTIC_ANALYSIS_BYTE_CODE_EMITTER_H
