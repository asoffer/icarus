#include "module/module.h"

#include "jasmin/serialization.h"
#include "nth/meta/sequence.h"
#include "nth/meta/type.h"
#include "serialization/module.pb.h"

namespace module {
namespace {

core::Type DeserializeType(serialization::Type const& proto) {
  return core::Type(proto.category(),
                    proto.has_value() ? proto.value() : proto.index());
}

void SerializeType(core::Type t, serialization::Type& proto,
                   bool inline_storage) {
  proto.set_category(t.category());
  if (inline_storage) {
    proto.set_value(t.index());
  } else {
    proto.set_index(t.index());
  }
}

void SerializeTypeSystem(semantic_analysis::TypeSystem& type_system,
                         serialization::TypeSystem& proto) {
  type_system.visit_all_stored([&](auto t) {
    using type_category_type     = std::decay_t<decltype(t)>;
    constexpr auto type_category = nth::type<type_category_type>;
    constexpr size_t CategoryIndex =
        semantic_analysis::TypeSystem::index<type_category_type>();

    if constexpr (type_category == nth::type<core::ParameterType>) {
      auto& parameters = *proto.add_parameters();
      for (auto const& parameter : t.value()) {
        auto& param = *parameters.add_parameters();
        param.set_name(parameter.name);
        SerializeType(
            parameter.value, *param.mutable_type(),
            type_system.has_inline_storage(parameter.value.category()));
      }
    } else if constexpr (type_category == nth::type<core::PointerType>) {
      SerializeType(t.pointee(), *proto.add_pointers(),
                    type_system.has_inline_storage(t.pointee().category()));
    } else if constexpr (type_category ==
                         nth::type<semantic_analysis::BufferPointerType>) {
      SerializeType(t.pointee(), *proto.add_buffer_pointers(),
                    type_system.has_inline_storage(t.pointee().category()));
    } else if constexpr (type_category ==
                         nth::type<semantic_analysis::ArrayType>) {
      auto& array = *proto.add_arrays();
      array.set_length(t.length());
      SerializeType(t.data_type(), *array.mutable_type(),
                    type_system.has_inline_storage(t.data_type().category()));
    } else if constexpr (type_category ==
                         nth::type<semantic_analysis::SliceType>) {
      SerializeType(t.pointee(), *proto.add_slices(),
                    type_system.has_inline_storage(t.pointee().category()));
    } else if constexpr (type_category == nth::type<core::FunctionType>) {
      auto& f = *proto.add_functions();
      f.set_parameters(t.parameter_type().index());
      f.mutable_return_types()->Reserve(t.returns().size());
      for (auto return_type : t.returns()) {
        SerializeType(return_type, *f.add_return_types(),
                      type_system.has_inline_storage(return_type.category()));
      }
    } else {
      static_assert(type_category.dependent(false));
    }
  });
}

void DeserializeTypeSystem(serialization::TypeSystem const& proto,
                           semantic_analysis::TypeSystem& type_system) {
  for (auto const& parameter_type : proto.parameters()) {
    core::Parameters<core::Type> parameters;
    parameters.reserve(parameter_type.parameters().size());
    for (auto const& p : parameter_type.parameters()) {
      parameters.append(p.name(), core::Type(DeserializeType(p.type())));
    }
    core::ParameterType(type_system, std::move(parameters));
  }

  for (auto const& t : proto.pointers()) {
    core::PointerType(type_system, DeserializeType(t));
  }

  for (auto const& t : proto.buffer_pointers()) {
    semantic_analysis::BufferPointerType(type_system, DeserializeType(t));
  }

  for (auto const& a : proto.arrays()) {
    semantic_analysis::ArrayType(type_system, a.length(),
                                 DeserializeType(a.type()));
  }

  for (auto const& t : proto.slices()) {
    semantic_analysis::SliceType(type_system, DeserializeType(t));
  }

  constexpr size_t ParameterTypeIndex =
      semantic_analysis::TypeSystem::index<core::ParameterType>();

  for (auto const& f : proto.functions()) {
    core::Type parameters(ParameterTypeIndex, f.parameters());
    std::vector<core::Type> returns;
    returns.reserve(f.return_types().size());
    for (auto const& r : f.return_types()) {
      returns.push_back(DeserializeType(r));
    }

    core::FunctionType(type_system,
                       parameters.get<core::ParameterType>(type_system),
                       std::move(returns));
  }
}

absl::flat_hash_map<void (*)(), std::pair<size_t, core::FunctionType>>
SerializeForeignSymbols(
    semantic_analysis::TypeSystem& type_system,
    semantic_analysis::ForeignFunctionMap const& map,
    google::protobuf::RepeatedPtrField<serialization::ForeignSymbol>& proto) {
  absl::flat_hash_map<void (*)(), std::pair<size_t, core::FunctionType>>
      index_map;
  for (auto const& [key, value] : map) {
    auto const& [name, type]    = key;
    auto const& [ir_fn, fn_ptr] = value;
    auto& symbol                = *proto.Add();
    index_map.try_emplace(fn_ptr, index_map.size(), type);
    symbol.set_name(name);
    SerializeType(type, *symbol.mutable_type(),
                  type_system.has_inline_storage(type.category()));
  }
  return index_map;
}

void DeserializeForeignSymbols(
    semantic_analysis::TypeSystem& type_system,
    google::protobuf::RepeatedPtrField<serialization::ForeignSymbol> const&
        proto,
    semantic_analysis::ForeignFunctionMap& map) {
  for (auto const& symbol : proto) {
    map.ForeignFunction(
        symbol.name(),
        DeserializeType(symbol.type()).get<core::FunctionType>(type_system));
  }
}

struct SerializationState {
  template <typename T>
  T& get() {
    return std::get<T>(state_);
  }

 private:
  std::tuple<semantic_analysis::PushFunction::serialization_state,
             semantic_analysis::InvokeForeignFunction::serialization_state,
             semantic_analysis::PushStringLiteral::serialization_state>
      state_;
};

void SerializeFunction(semantic_analysis::IrFunction const& f,
                       serialization::Function& proto,
                       SerializationState& state) {
  proto.set_parameters(f.parameter_count());
  proto.set_returns(f.return_count());
  jasmin::Serialize(f, *proto.mutable_content(), state);
}

void SerializeReadOnlyData(
    serialization::ReadOnlyData& data,
    semantic_analysis::PushStringLiteral::serialization_state const& state) {
  for (std::string_view content : state) {
    *data.add_strings() = std::string(content);
  }
}

void DeserializeReadOnlyData(
    serialization::ReadOnlyData const& data,
    semantic_analysis::PushStringLiteral::serialization_state& state) {
  for (std::string_view content : data.strings()) { state.index(content); }
}

}  // namespace

bool Module::Serialize(std::ostream& output) const {
  serialization::Module proto;

  *proto.mutable_identifier() = id_.value();
  SerializeTypeSystem(type_system(), *proto.mutable_type_system());

  SerializationState state;

  auto& foreign_state =
      state
          .get<semantic_analysis::InvokeForeignFunction::serialization_state>();
  foreign_state.set_foreign_function_map(&foreign_function_map());
  foreign_state.set_type_system(type_system());
  foreign_state.set_map(SerializeForeignSymbols(
      type_system(), foreign_function_map(), *proto.mutable_foreign_symbols()));

  SerializeFunction(initializer_, *proto.mutable_initializer(), state);
  proto.mutable_functions()->Reserve(functions_.size());

  auto& f_state =
      state.get<semantic_analysis::PushFunction::serialization_state>();

  for (auto& function : functions_) { f_state.index(&function); }
  size_t serialized_up_to = 0;
  // TODO look up iterator invalidation constraints to see if we can process
  // more than one at a time.
  while (serialized_up_to != f_state.size()) {
    auto const& f = **(f_state.begin() + serialized_up_to);
    SerializeFunction(f, *proto.add_functions(), state);
    ++serialized_up_to;
  }

  SerializeReadOnlyData(
      *proto.mutable_read_only(),
      state.get<semantic_analysis::PushStringLiteral::serialization_state>());
  data_types::Serialize(integer_table_, *proto.mutable_integers());

  auto& exported = *proto.mutable_exported();
  for (auto const & [name, symbols] : exported_symbols_) {
    auto& exported_symbols = exported[name];
    for (auto const& symbol : symbols) {
      auto& exported_symbol = *exported_symbols.add_symbols();
      SerializeType(
          semantic_analysis::Type, *exported_symbol.mutable_symbol_type(),
          type_system().has_inline_storage(semantic_analysis::Type.category()));
      SerializeType(
          symbol.as<core::Type>(), *exported_symbol.mutable_type(),
          type_system().has_inline_storage(symbol.as<core::Type>().category()));
    }
  }
  return proto.SerializeToOstream(&output);
}

bool Module::DeserializeInto(serialization::Module proto, Module& module) {
  SerializationState state;

  data_types::Deserialize(proto.integers(), module.integer_table_);
  DeserializeReadOnlyData(
      module.read_only_data(),
      state.get<semantic_analysis::PushStringLiteral::serialization_state>());

  DeserializeTypeSystem(proto.type_system(), module.type_system_);

  auto& exported = *proto.mutable_exported();
  for (auto const& [name, symbols] : proto.exported()) {
    for (auto const& symbol : symbols.symbols()) {
      core::Type symbol_type = DeserializeType(symbol.symbol_type());
      if (symbol_type == semantic_analysis::Type) {
        module.exported_symbols_[name].push_back(
            DeserializeType(symbol.type()));
      } else {
        NOT_YET();
      }
    }
  }

  DeserializeForeignSymbols(module.type_system(), *proto.mutable_foreign_symbols(),
                            module.foreign_function_map_);

  // Populate the PushFunction state map.
  auto& f_state =
      state.get<semantic_analysis::PushFunction::serialization_state>();
  for (auto const& function : proto.functions()) {
    auto [id, f_ptr] =
        module.create_function(function.parameters(), function.returns());
    size_t index = f_state.index(f_ptr);
    ASSERT(id.local().value() == index);
  }

  auto& foreign_state =
      state
          .get<semantic_analysis::InvokeForeignFunction::serialization_state>();
  foreign_state.set_foreign_function_map(&module.foreign_function_map());
  foreign_state.set_type_system(module.type_system());

  jasmin::Deserialize(proto.initializer().content(), module.initializer_, state);
  size_t fn_index = 0;
  for (auto const& function : proto.functions()) {
    jasmin::Deserialize(function.content(), module.functions_[fn_index], state);
    ++fn_index;
  }

  return true;
}

}  // namespace module
