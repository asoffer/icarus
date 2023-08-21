#include "nth/container/flyweight_map.h"
#include "semantic_analysis/byte_code/emitter.h"

namespace semantic_analysis {

void ByteCodeValueEmitter::operator()(ast::ShortFunctionLiteral const* node,
                                      FunctionData data) {
  auto const& function_type =
      context().qualified_type(node).type().get<core::FunctionType>(
          GlobalTypeSystem);
  size_t num_parameters = function_type.parameters().size();

  auto [fn_index, fn_ptr] = module().function_table().emplace(
      num_parameters, function_type.returns().size(),
      resources().primary_module().id());

  nth::flyweight_map<ast::Declaration::Id const*, size_t> variable_offsets;
  // Populate `variable_offsets`
  core::Bytes offset{};
  node->body_scope().ForEachNonConstantDeclaration(
      [&](ast::Declaration const* decl) {
        for (auto& id : decl->ids()) {
          variable_offsets.try_emplace(&id, offset.value());
          // TODO: Alignment.
          offset += SizeOf(context().qualified_type(&id).type());
        }
      });
  fn_ptr->AppendStackAllocate(offset.value());

  size_t parameter_index = 0;
  node->body_scope().ForEachNonConstantDeclaration(
      [&, fn_ptr = fn_ptr](ast::Declaration const* decl) {
        if (decl->flags() & ast::Declaration::f_IsFnParam) {
          for (auto& id : decl->ids()) {
            auto iter = variable_offsets.find(&id);
            core::Bytes parameter_size =
                SizeOf(function_type.parameters()[parameter_index].value);
            if (parameter_size.value() <= jasmin::ValueSize) {
              fn_ptr->AppendStackOffset(iter->second);
              fn_ptr->AppendDuplicateAt(num_parameters - parameter_index);
              fn_ptr->AppendStore(parameter_size.value());
            } else if (function_type.parameters()[parameter_index]
                           .value.is<SliceType>(GlobalTypeSystem)) {
              fn_ptr->AppendStackOffset(iter->second);
              fn_ptr->AppendIncrementPointer(jasmin::ValueSize);
              fn_ptr->AppendSwap();
              fn_ptr->AppendStore(jasmin::ValueSize);

              fn_ptr->AppendStackOffset(iter->second);
              fn_ptr->AppendSwap();
              fn_ptr->AppendStore(jasmin::ValueSize);
            } else {
              NTH_UNIMPLEMENTED();
            }
            ++parameter_index;
          }
        }
      });

  FunctionData fn_data(*fn_ptr, variable_offsets);
  Emit(node->body(), fn_data);
  fn_data.function().AppendReturn();

  data.function().AppendPushFunction(fn_ptr);
}

void ByteCodeStatementEmitter::operator()(ast::ShortFunctionLiteral const*,
                                          FunctionData) {}

}  // namespace semantic_analysis