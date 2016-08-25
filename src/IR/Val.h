#ifndef ICARUS_IR_VAL_H
#define ICARUS_IR_VAL_H

struct Type;

namespace IR {
// Forward declarations

#define VAL_MACRO(TypeName, type_name, cpp_type) struct TypeName##Val;
#include "../config/val.conf"
#undef VAL_MACRO

struct Val {
  Val() {}
  virtual ~Val() {}
  virtual Val *clone() const = 0;
  virtual llvm::Constant *llvm() = 0;
  virtual std::string to_string() const = 0;

#define VAL_MACRO(TypeName, type_name, cpp_type)                               \
  virtual bool is_##type_name() const { return false; }                        \
  cpp_type Get##TypeName() const;

#include "../config/val.conf"
#undef VAL_MACRO

protected:
  Val(const Val &v) = default;
};

Order ArbitraryOrdering(const Val *lhs, const Val *rhs);

#define VAL_MACRO(TypeName, type_name, cpp_type)                               \
  struct TypeName##Val : Val {                                                 \
    TypeName##Val() {}                                                         \
    virtual ~TypeName##Val() {}                                                \
    virtual TypeName##Val *clone() const { return new TypeName##Val(*this); }  \
    bool is_##type_name() const { return true; }                               \
    llvm::Constant *llvm();                                                    \
    std::string to_string() const;                                             \
    cpp_type val;                                                              \
  };

#include "../config/val.conf"
#undef VAL_MACRO
} // namespace IR

#endif // ICARUS_IR_VAL_H
