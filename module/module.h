#ifndef ICARUS_MODULE_MODULE_H
#define ICARUS_MODULE_MODULE_H

#include <deque>
#include <istream>
#include <optional>
#include <ostream>

#include "data_types/integer.h"
#include "module/symbol.h"
#include "semantic_analysis/instruction_set.h"
#include "semantic_analysis/type_system.h"
#include "serialization/foreign_symbol_map.h"
#include "serialization/function_table.h"
#include "serialization/module.pb.h"
#include "serialization/module_index.h"
#include "serialization/module_map.h"
#include "serialization/read_only_data.h"

namespace module {

struct Module {
  explicit Module(serialization::UniqueModuleId id) : id_(std::move(id)) {}

  bool Serialize(std::ostream &output) const;
  static bool DeserializeInto(serialization::Module proto, Module &module);

  semantic_analysis::IrFunction &initializer() { return initializer_; }
  semantic_analysis::IrFunction const &initializer() const {
    return initializer_;
  }

  semantic_analysis::TypeSystem &type_system() const { return type_system_; }

  serialization::ForeignSymbolMap const &foreign_symbol_map() const {
    return foreign_symbol_map_;
  }
  serialization::ForeignSymbolMap &foreign_symbol_map() {
    return foreign_symbol_map_;
  }

  data_types::IntegerTable &integer_table() { return integer_table_; }
  data_types::IntegerTable const &integer_table() const {
    return integer_table_;
  }

  std::span<Symbol const> LoadSymbols(std::string_view name) const {
    auto iter = exported_symbols_.find(name);
    if (iter == exported_symbols_.end()) { return {}; }
    return iter->second;
  }

  void Export(std::string_view name, Symbol s) {
    exported_symbols_[name].push_back(std::move(s));
  }

  void Export(std::string_view name, core::Type t,
              semantic_analysis::IrFunction const *f) {
    Export(name, Symbol(TypedFunction{
                     .type     = t,
                     .function = function_table().find(f),
                 }));
  }

  auto const &read_only_data() const { return read_only_data_; }

  serialization::FunctionTable<semantic_analysis::IrFunction> const &
  function_table() const {
    return function_table_;
  }
  serialization::FunctionTable<semantic_analysis::IrFunction>
      &function_table() {
    return function_table_;
  }

  serialization::ModuleMap const &map() const { return module_map_; }
  serialization::ModuleMap &map() { return module_map_; }

  std::pair<serialization::FunctionIndex, semantic_analysis::IrFunction const *>
  Wrap(serialization::ModuleIndex index,
       semantic_analysis::IrFunction const *f);

 private:
  serialization::UniqueModuleId id_;

  // Accepts two arguments (a slice represented as data followed by length).
  semantic_analysis::IrFunction initializer_{2, 0};

  absl::flat_hash_map<std::string, std::vector<Symbol>> exported_symbols_;

  // The type-system containing all types referenceable in this module.
  mutable semantic_analysis::TypeSystem type_system_;
  // TODO: Mutable because jasmin doesn't correctly pass const qualifiers
  // through `get`. Remove when that's fixed.
  mutable serialization::ForeignSymbolMap foreign_symbol_map_{&type_system_};
  mutable serialization::FunctionTable<semantic_analysis::IrFunction>
      function_table_;

  // Keyed on functions in other modules. The mapped value is the index in
  // `function_table()` of the wrapper.
  absl::flat_hash_map<semantic_analysis::IrFunction const *, size_t>
      wrapped_;

  // All integer constants used in the module.
  data_types::IntegerTable integer_table_;

  mutable serialization::ReadOnlyData read_only_data_;
  serialization::ModuleMap module_map_;
};

}  // namespace module

#endif  // ICARUS_MODULE_MODULE_H
