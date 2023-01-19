#ifndef ICARUS_MODULE_MODULE_H
#define ICARUS_MODULE_MODULE_H

#include <deque>
#include <istream>
#include <optional>
#include <ostream>

#include "module/data/integer_table.h"
#include "semantic_analysis/foreign_function_map.h"
#include "semantic_analysis/instruction_set.h"
#include "semantic_analysis/type_system.h"

namespace module {

struct Module {
  bool Serialize(std::ostream &output) const;
  static std::optional<Module> Deserialize(std::istream &input);

  semantic_analysis::IrFunction &initializer() { return initializer_; }
  semantic_analysis::IrFunction const &initializer() const {
    return initializer_;
  }

  semantic_analysis::TypeSystem &type_system() { return type_system_; }
  semantic_analysis::TypeSystem &type_system() const { return type_system_; }

  semantic_analysis::ForeignFunctionMap const &foreign_function_map() const {
    return foreign_function_map_;
  }

  semantic_analysis::ForeignFunctionMap &foreign_function_map() {
    return foreign_function_map_;
  }

  IntegerTable &integer_table() { return integer_table_; }
  IntegerTable const &integer_table() const { return integer_table_; }

  semantic_analysis::IrFunction const *function(ir::Fn fn_id) const {
    if (fn_id.module() == ir::ModuleId::Foreign()) {
      return foreign_function_map_.ForeignFunction(fn_id.local());
    } else if (fn_id.module() == ir::ModuleId(0)) {
      size_t index = fn_id.local().value();
      ASSERT(index < functions_.size());
      return &functions_[index];
    } else {
      NOT_YET();
    }
  }

  std::pair<ir::Fn, semantic_analysis::IrFunction *> create_function(
      size_t parameters, size_t returns) {
    ir::Fn fn(ir::ModuleId(0), ir::LocalFnId(functions_.size()));
    return std::pair(fn, &functions_.emplace_back(parameters, returns));
  }

 private:
  // Accepts two arguments (a slice represented as data followed by length).
  semantic_analysis::IrFunction initializer_{2, 0};

  // The type-system containing all types referenceable in this module.
  mutable semantic_analysis::TypeSystem type_system_;
  semantic_analysis::ForeignFunctionMap foreign_function_map_{type_system_};
  std::deque<semantic_analysis::IrFunction> functions_;

  // All integer constants used in the module.
  IntegerTable integer_table_;
};

}  // namespace module

#endif  // ICARUS_MODULE_MODULE_H
