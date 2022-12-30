#include "ir/value/module_id.h"
#include "semantic_analysis/byte_code/emitter.h"

namespace semantic_analysis {

void ByteCodeValueEmitter::operator()(ast::Access const* node,
                                      FunctionData data) {
  QualifiedType operand_qt = context().qualified_type(node->operand());
  if (operand_qt.type().get_if<SliceType>(type_system())) {
    Emit(node->operand(), data);
    if (node->member_name() == "data") {
      if (operand_qt.qualifiers() >= Qualifiers::Reference()) {
        NOT_YET();
      } else {
        data.function().append<jasmin::Drop>(1);
      }
    } else if (node->member_name() == "length") {
      if (operand_qt.qualifiers() >= Qualifiers::Reference()) {
        NOT_YET();
      } else {
        data.function().append<jasmin::Swap>();
        data.function().append<jasmin::Drop>(1);
      }
    }
  }
}

void ByteCodeStatementEmitter::operator()(ast::Access const* node,
                                          FunctionData data) {
  Emit(node->operand(), data);
}

}  // namespace semantic_analysis