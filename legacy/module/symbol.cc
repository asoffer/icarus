#include "module/symbol.h"

#include "nth/debug/debug.h"
#include "semantic_analysis/type_system.h"

namespace module {

core::Type Symbol::type() const {
  switch (symbol_.index()) {
    case 0: return semantic_analysis::Type;
    case 1: return std::get<TypedFunction>(symbol_).type;
    case 2: return std::get<TypedValue>(symbol_).type;
    default: NTH_UNREACHABLE();
  }
}

}  // namespace module