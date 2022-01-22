#ifndef ICARUS_TYPE_POINTER_H
#define ICARUS_TYPE_POINTER_H

#include "base/extend.h"
#include "base/extend/serialize.h"
#include "base/extend/traverse.h"
#include "ir/instruction/base.h"
#include "ir/instruction/debug.h"
#include "type/type.h"

namespace type {

// `Pointer` is a type representing the address of an object of the given
// pointed-to type (the `pointee`).
struct Pointer : LegacyType {
  friend Pointer const *Ptr(Type t);

  void WriteTo(std::string *buf) const override;
  core::Bytes bytes(core::Arch const &arch) const override;
  core::Alignment alignment(core::Arch const &arch) const override;

  bool is_big() const override { return false; }

  Type pointee() const { return pointee_; }

  bool EqualsValue(ir::CompleteResultRef const &lhs,
                   ir::CompleteResultRef const &rhs) const override;
  size_t HashValue(ir::CompleteResultRef const &value) const override;
  void ShowValue(std::ostream &os,
                 ir::CompleteResultRef const &value) const override;

  Completeness completeness() const override { return Completeness::Complete; }

 protected:
  Pointer(Type t) : Pointer(IndexOf<Pointer>(), t) {}

  Pointer(int8_t which, Type t)
      : LegacyType(which, LegacyType::Flags{.is_default_initializable = 1,
                                            .is_copyable              = 1,
                                            .is_movable               = 1,
                                            .has_destructor           = 0}),
        pointee_(t) {}

 private:
  Type pointee_;
};

// `BufferPointer` is a type representing the address of an object, inside an
// a contiguous block of objects of that type. It is similar to `Pointer`, but
// it also supports arithmetic. `BufferPointer`s are implicitly convertible to
// `Pointer`s with the same `pointee` type.
struct BufferPointer : Pointer {
  friend BufferPointer const *BufPtr(Type t);

  bool is_big() const override { return false; }
  void WriteTo(std::string *result) const override;

  Completeness completeness() const override { return Completeness::Complete; }

 private:
  BufferPointer() = delete;
  BufferPointer(Type t) : Pointer(IndexOf<BufferPointer>(), t) {}
};

Pointer const *Ptr(Type t);
BufferPointer const *BufPtr(Type t);

struct PtrInstruction
    : base::Extend<PtrInstruction>::With<base::BaseSerializeExtension,
                                         base::BaseTraverseExtension,
                                         ir::DebugFormatExtension> {
  static constexpr std::string_view kDebugFormat = "%2$s = ptr %1$s";

  Type Resolve() const { return Apply(operand.value()); }
  static type::Type Apply(type::Type operand) { return type::Ptr(operand); }

  ir::RegOr<type::Type> operand;
  ir::Reg result;
};

struct BufPtrInstruction
    : base::Extend<BufPtrInstruction>::With<base::BaseSerializeExtension,
                                            base::BaseTraverseExtension,
                                            ir::DebugFormatExtension> {
  static constexpr std::string_view kDebugFormat = "%2$s = buf-ptr %1$s";

  Type Resolve() const { return Apply(operand.value()); }
  static type::Type Apply(type::Type operand) { return type::BufPtr(operand); }

  ir::RegOr<type::Type> operand;
  ir::Reg result;
};

}  // namespace type

#endif  // ICARUS_TYPE_POINTER_H
