#ifndef ICARUS_IR_VAL_H
#define ICARUS_IR_VAL_H

#include <functional>
#include <iostream>
#include <memory>
#include <variant>

#include "base/strong_types.h"
#include "base/types.h"
#include "ir/interface.h"
#include "ir/register.h"
#include "type/type.h"

struct Module;

namespace type {
struct Enum;
struct Flags;
struct Pointer;
struct Struct;
}  // namespace type

namespace AST {
struct Expression;
struct ScopeLiteral;
struct BlockLiteral;
struct Function;
struct CodeBlock;
}  // namespace AST

namespace IR {
inline FlagsVal operator|(FlagsVal lhs, FlagsVal rhs) {
  return FlagsVal{lhs.value | rhs.value};
}
inline FlagsVal operator^(FlagsVal lhs, FlagsVal rhs) {
  return FlagsVal{lhs.value ^ rhs.value};
}
inline FlagsVal operator&(FlagsVal lhs, FlagsVal rhs) {
  return FlagsVal{lhs.value & rhs.value};
}

struct Func;
}  // namespace IR

namespace IR {
struct Val {
  const type::Type *type = nullptr;
  // TODO make trivial: interface
  std::variant<Register, IR::Addr, bool, char, double, i32, EnumVal, FlagsVal,
               const type::Type *, type::Struct *, IR::Func *, AST::Function *,
               AST::ScopeLiteral *, IR::Interface, AST::Expression *,
               BlockIndex, std::string_view, const Module *, BlockSequence,
               BuiltinGenericIndex, ForeignFn>
      value{false};

  template <typename T>
  RegisterOr<T> reg_or() const {
    if (auto *r = std::get_if<Register>(&value)) {
      return RegisterOr<T>(*r);
    } else {
      return RegisterOr<T>(std::get<T>(value));
    }
  }

  template <typename T,
            typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, Val>>>
  explicit Val(T &&val)
      : Val(::type::Get<std::decay_t<T>>(), std::forward<T>(val)) {}

  explicit Val(std::nullptr_t)
      : Val(type::NullPtr, IR::Addr{Addr::Kind::Null, 0}) {}
  explicit Val(AST::ScopeLiteral *scope_lit);

  static Val Reg(Register r, const type::Type *t) { return Val(t, r); }
  static Val BuiltinGeneric(i32 n) {
    return Val(type::Generic, BuiltinGenericIndex{n});
  }
  static Val Enum(const type::Enum *enum_type, size_t integral_val);
  static Val Flags(const type::Flags *flags_type, FlagsVal val);
  static Val CodeBlock(const AST::CodeBlock &block);
  static Val Foreign(const type::Type *t, ForeignFn f) { return Val(t, f); }
  static Val Func(IR::Func *fn);  // TODO deprecate?
  static Val Func(AST::Function *fn);
  static Val BasicBlock(BlockIndex bi) { return Val(nullptr, bi); }
  static Val Block(AST::BlockLiteral *b);
  static Val BlockSeq(BlockSequence b);
  static Val Null(const type::Type *t);
  static Val Interface(IR::Interface ifc);

  static Val CharBuf(const std::string &str);
  static Val Ref(AST::Expression *expr);
  static Val None() { return Val(); }

  std::string to_string() const;

  Val()                = default;
  ~Val() noexcept      = default;
  Val(const Val &)     = default;
  Val(Val &&) noexcept = default;
  Val &operator=(const Val &) = default;
  Val &operator=(Val &&) noexcept = default;

 private:
  friend Val ValFrom(RegisterOr<IR::Addr> r, type::Pointer const *ptr_type);

  template <typename T>
  Val(const type::Type *t, T &&val) : type(t), value(std::forward<T>(val)) {}
};

template <typename T>
inline Val ValFrom(RegisterOr<T> r) {
  return r.is_reg_ ? Val::Reg(r.reg_, type::Get<T>()) : Val(r.val_);
}

Val ValFrom(RegisterOr<FlagsVal> r, type::Flags const *t);
Val ValFrom(RegisterOr<IR::Addr> r, type::Pointer const *ptr_type);

inline bool operator==(const Val &lhs, const Val &rhs) {
  return lhs.type == rhs.type && lhs.value == rhs.value;
}
inline bool operator!=(const Val &lhs, const Val &rhs) { return !(lhs == rhs); }
bool operator<(const ::IR::Val &lhs, const ::IR::Val &rhs);
}  // namespace IR

#endif  // ICARUS_IR_VAL_H
