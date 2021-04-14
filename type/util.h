#ifndef ICARUS_TYPE_UTIL_H
#define ICARUS_TYPE_UTIL_H
// TODO this file is terribly named.

#include "base/no_destructor.h"
#include "ir/value/addr.h"
#include "ir/value/block.h"
#include "ir/value/char.h"
#include "ir/value/module_id.h"
#include "ir/value/reg.h"
#include "ir/value/scope.h"
#include "ir/value/string.h"
#include "ir/value/value.h"
#include "type/array.h"
#include "type/enum.h"
#include "type/flags.h"
#include "type/function.h"
#include "type/generic_function.h"
#include "type/jump.h"
#include "type/opaque.h"
#include "type/pointer.h"
#include "type/primitive.h"
#include "type/struct.h"
#include "type/type.h"
#include "type/typed_value.h"

namespace ir {
struct Jump;
struct Fn;
}  // namespace ir

namespace type {

template <typename T>
type::Type Get() {
  if constexpr (base::meta<T> == base::meta<bool>) {
    return type::Bool;
  } else if constexpr (base::meta<T> == base::meta<ir::Char>) {
    return type::Char;
  } else if constexpr (base::meta<T> == base::meta<int8_t>) {
    return type::I8;
  } else if constexpr (base::meta<T> == base::meta<int16_t>) {
    return type::I16;
  } else if constexpr (base::meta<T> == base::meta<int32_t>) {
    return type::I32;
  } else if constexpr (base::meta<T> == base::meta<int64_t>) {
    return type::I64;
  } else if constexpr (base::meta<T> == base::meta<uint8_t>) {
    return type::U8;
  } else if constexpr (base::meta<T> == base::meta<uint16_t>) {
    return type::U16;
  } else if constexpr (base::meta<T> == base::meta<uint32_t>) {
    return type::U32;
  } else if constexpr (base::meta<T> == base::meta<uint64_t>) {
    return type::U64;
  } else if constexpr (base::meta<T> == base::meta<float>) {
    return type::F32;
  } else if constexpr (base::meta<T> == base::meta<double>) {
    return type::F64;
  } else if constexpr (base::meta<T> == base::meta<ir::Block>) {
    return type::Block;
  } else if constexpr (base::meta<T> == base::meta<type::Type>) {
    return type::Type_;
  } else if constexpr (base::meta<T> == base::meta<ir::Scope>) {
    return type::Scope;
  } else if constexpr (base::meta<T> == base::meta<ir::ModuleId>) {
    return type::Module;
  } else if constexpr (std::is_pointer_v<T>) {
    return Ptr(Get<std::decay_t<decltype(*std::declval<T>())>>());
  } else {
    UNREACHABLE(typeid(T).name());
  }
}

template <typename T>
bool Compare(::type::Type t) {
  if constexpr (base::meta<T> == base::meta<type::Enum::underlying_type>) {
    if (t.is<type::Enum>()) { return true; }
  }

  if constexpr (base::meta<T> == base::meta<type::Flags::underlying_type>) {
    if (t.is<type::Flags>()) { return true; }
  }

  if constexpr (base::meta<T> == base::meta<bool>) {
    return t == ::type::Bool;
  } else if constexpr (base::meta<T> == base::meta<ir::Char>) {
    return t == ::type::Char;
  } else if constexpr (base::meta<T> == base::meta<int8_t>) {
    return t == ::type::I8;
  } else if constexpr (base::meta<T> == base::meta<int16_t>) {
    return t == ::type::I16;
  } else if constexpr (base::meta<T> == base::meta<int32_t>) {
    return t == ::type::I32;
  } else if constexpr (base::meta<T> == base::meta<int64_t>) {
    return t == ::type::I64;
  } else if constexpr (base::meta<T> == base::meta<uint8_t>) {
    return t == ::type::U8;
  } else if constexpr (base::meta<T> == base::meta<uint16_t>) {
    return t == ::type::U16;
  } else if constexpr (base::meta<T> == base::meta<uint32_t>) {
    return t == ::type::U32;
  } else if constexpr (base::meta<T> == base::meta<uint64_t>) {
    return t == ::type::U64;
  } else if constexpr (base::meta<T> == base::meta<float>) {
    return t == ::type::F32;
  } else if constexpr (base::meta<T> == base::meta<double>) {
    return t == ::type::F64;
  } else if constexpr (base::meta<T> == base::meta<::type::Type>) {
    return t == ::type::Type_;
  } else if constexpr (base::meta<T> == base::meta<::type::Struct const *>) {
    return t.is<::type::Struct>();
  } else if constexpr (base::meta<T> == base::meta<ir::Addr>) {
    return t.is<::type::Pointer>() or t == type::NullPtr;
  } else if constexpr (base::meta<T> == base::meta<ir::Scope>) {
    return t == ::type::Scope;
  } else if constexpr (base::meta<T> == base::meta<::type::Struct const *>) {
    return t.is<::type::Struct>();
  } else if constexpr (base::meta<T> == base::meta<ir::Fn>) {
    return t.is<::type::Function>();
  } else if constexpr (base::meta<T> == base::meta<ir::Jump>) {
    return t.is<::type::Jump>();
  } else if constexpr (base::meta<T> == base::meta<ir::GenericFn>) {
    return t.is<::type::GenericFunction>();
  } else if constexpr (base::meta<T> == base::meta<ir::ModuleId>) {
    return t == ::type::Module;
  } else if constexpr (base::meta<T> == base::meta<ir::Block>) {
    return t == ::type::Block;
  } else {
    UNREACHABLE(t.to_string(), " vs ", typeid(T).name());
  }
}

template <typename... Ts, typename Fn>
auto ApplyTypes(Type t, Fn &&fn) {
  using return_type = decltype(fn.template operator()<base::first_t<Ts...>>());

  // Create a static array of funtions that may be called depending on which
  // type matches.
  static base::NoDestructor kFnToCall =
      std::array<return_type (*)(Fn &&), sizeof...(Ts)>{
          [](Fn &&f) { return f.template operator()<Ts>(); }...};

  // Using fold expressions, take the disjunction of `type::Compare<T>(t)` over
  // all T. This will compute this boolean value until it returns true. However,
  // in each folded expression, we actually use the comma operator to first
  // increment `index`, which means that `index` will be incremented the number
  // until `type::Compare<T>(t)` returns true. This means that after the
  // computation, the value of `index` is one more than the array index for the
  // function we want to call.
  size_t index = 0;
  bool found   = ((++index, type::Compare<Ts>(t)) or ...);
  ASSERT(found == true);

  return (*kFnToCall)[index - 1](std::forward<Fn>(fn));
}

template <typename Fn>
auto Apply(Type t, Fn &&fn) {
  return ApplyTypes<bool, ir::Char, int8_t, int16_t, int32_t, int64_t, uint8_t,
                    uint16_t, uint32_t, uint64_t, float, double, type::Type,
                    ir::Addr, ir::ModuleId, ir::Scope, ir::Fn, ir::Jump,
                    ir::Block, ir::GenericFn>(t, std::forward<Fn>(fn));
}

}  // namespace type

#endif  // ICARUS_TYPE_UTIL_H
