#include "semantic_analysis/byte_code/byte_code.h"
#include "semantic_analysis/byte_code/emitter.h"

#include "compiler/context.h"

namespace semantic_analysis {

IrFunction EmitByteCode(ast::Expression const &expression,
                        compiler::Context const &context) {
  IrFunction f(0, 1);
  ByteCodeValueEmitter e(&context);
  e.EmitByteCode(&expression, f);
  f.append<jasmin::Return>();
  return f;
}

}  // namespace semantic_analysis
