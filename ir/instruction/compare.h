#ifndef ICARUS_IR_INSTRUCTION_COMPARE_H
#define ICARUS_IR_INSTRUCTION_COMPARE_H

#include <string_view>

#include "base/extend.h"
#include "base/extend/serialize.h"
#include "base/extend/traverse.h"
#include "ir/instruction/debug.h"
#include "ir/interpreter/execution_context.h"
#include "ir/interpreter/legacy_stack_frame.h"
#include "ir/value/reg.h"
#include "ir/value/reg_or.h"

namespace ir {

template <typename NumType>
struct EqInstruction : base::Extend<EqInstruction<NumType>>::template With<
                           base::BaseTraverseExtension,
                           base::BaseSerializeExtension, DebugFormatExtension> {
  using num_type = NumType;
  using operand_type =
      std::conditional_t<::interpreter::FitsInRegister<num_type>, num_type,
                         addr_t>;
  static constexpr std::string_view kDebugFormat = "%3$s = eq %1$s %2$s";

  friend bool InterpretInstruction(EqInstruction<num_type> const &inst,
                                   interpreter::StackFrame &frame) {
    bool value;
    if constexpr (::interpreter::FitsInRegister<num_type>) {
      value = (frame.resolve(inst.lhs) == frame.resolve(inst.rhs));
    } else {
      value = (*reinterpret_cast<num_type const *>(frame.resolve(inst.lhs)) ==
               *reinterpret_cast<num_type const *>(frame.resolve(inst.rhs)));
    }

    frame.set(inst.result, value);
    return true;
  }

  void Apply(::interpreter::ExecutionContext &ctx) const
      requires(not ::interpreter::FitsInRegister<num_type>) {
    ctx.current_frame().set(
        result,
        (*reinterpret_cast<num_type const *>(ctx.resolve<addr_t>(lhs)) ==
         *reinterpret_cast<num_type const *>(ctx.resolve<addr_t>(rhs))));
  }

  bool Resolve() const requires(::interpreter::FitsInRegister<num_type>) {
    return Apply(lhs.value(), rhs.value());
  }
  static bool Apply(num_type lhs, num_type rhs) { return lhs == rhs; }

  RegOr<operand_type> lhs;
  RegOr<operand_type> rhs;
  Reg result;
};

template <typename NumType>
struct NeInstruction : base::Extend<NeInstruction<NumType>>::template With<
                           base::BaseTraverseExtension,
                           base::BaseSerializeExtension, DebugFormatExtension> {
  using num_type = NumType;
  using operand_type =
      std::conditional_t<::interpreter::FitsInRegister<num_type>, num_type,
                         addr_t>;
  static constexpr std::string_view kDebugFormat = "%3$s = ne %1$s %2$s";

  friend bool InterpretInstruction(NeInstruction<num_type> const &inst,
                                   interpreter::StackFrame &frame) {
    bool value;
    if constexpr (::interpreter::FitsInRegister<num_type>) {
      value = (frame.resolve(inst.lhs) != frame.resolve(inst.rhs));
    } else {
      value = (*reinterpret_cast<num_type const *>(frame.resolve(inst.lhs)) !=
               *reinterpret_cast<num_type const *>(frame.resolve(inst.rhs)));
    }

    frame.set(inst.result, value);
    return true;
  }

  void Apply(::interpreter::ExecutionContext &ctx) const
      requires(not ::interpreter::FitsInRegister<num_type>) {
    ctx.current_frame().set(
        result,
        (*reinterpret_cast<num_type const *>(ctx.resolve<addr_t>(lhs)) !=
         *reinterpret_cast<num_type const *>(ctx.resolve<addr_t>(rhs))));
  }

  bool Resolve() const requires(::interpreter::FitsInRegister<num_type>) {
    return Apply(lhs.value(), rhs.value());
  }
  static bool Apply(num_type lhs, num_type rhs) { return lhs != rhs; }

  RegOr<operand_type> lhs;
  RegOr<operand_type> rhs;
  Reg result;
};

template <typename NumType>
struct LtInstruction : base::Extend<LtInstruction<NumType>>::template With<
                           base::BaseTraverseExtension,
                           base::BaseSerializeExtension, DebugFormatExtension> {
  using num_type = NumType;
  using operand_type =
      std::conditional_t<::interpreter::FitsInRegister<num_type>, num_type,
                         addr_t>;
  static constexpr std::string_view kDebugFormat = "%3$s = lt %1$s %2$s";

  friend bool InterpretInstruction(LtInstruction<num_type> const &inst,
                                   interpreter::StackFrame &frame) {
    bool value;
    if constexpr (::interpreter::FitsInRegister<num_type>) {
      value = (frame.resolve(inst.lhs) < frame.resolve(inst.rhs));
    } else {
      value = (*reinterpret_cast<num_type const *>(frame.resolve(inst.lhs)) <
               *reinterpret_cast<num_type const *>(frame.resolve(inst.rhs)));
    }

    frame.set(inst.result, value);
    return true;
  }

  void Apply(::interpreter::ExecutionContext &ctx) const
      requires(not ::interpreter::FitsInRegister<num_type>) {
    ctx.current_frame().set(
        result,
        (*reinterpret_cast<num_type const *>(ctx.resolve<addr_t>(lhs)) <
         *reinterpret_cast<num_type const *>(ctx.resolve<addr_t>(rhs))));
  }

  bool Resolve() const requires(::interpreter::FitsInRegister<num_type>) {
    return Apply(lhs.value(), rhs.value());
  }
  static bool Apply(num_type lhs, num_type rhs) { return lhs < rhs; }

  RegOr<operand_type> lhs;
  RegOr<operand_type> rhs;
  Reg result;
};

template <typename NumType>
struct LeInstruction : base::Extend<LeInstruction<NumType>>::template With<
                           base::BaseTraverseExtension,
                           base::BaseSerializeExtension, DebugFormatExtension> {
  using num_type = NumType;
  using operand_type =
      std::conditional_t<::interpreter::FitsInRegister<num_type>, num_type,
                         addr_t>;
  static constexpr std::string_view kDebugFormat = "%3$s = le %1$s %2$s";

  friend bool InterpretInstruction(LeInstruction<num_type> const &inst,
                                   interpreter::StackFrame &frame) {
    bool value;
    if constexpr (::interpreter::FitsInRegister<num_type>) {
      value = (frame.resolve(inst.lhs) <= frame.resolve(inst.rhs));
    } else {
      value = (*reinterpret_cast<num_type const *>(frame.resolve(inst.lhs)) <=
               *reinterpret_cast<num_type const *>(frame.resolve(inst.rhs)));
    }

    frame.set(inst.result, value);
    return true;
  }

  void Apply(::interpreter::ExecutionContext &ctx) const
      requires(not ::interpreter::FitsInRegister<num_type>) {
    ctx.current_frame().set(
        result,
        (*reinterpret_cast<num_type const *>(ctx.resolve<addr_t>(lhs)) <=
         *reinterpret_cast<num_type const *>(ctx.resolve<addr_t>(rhs))));
  }

  bool Resolve() const requires(::interpreter::FitsInRegister<num_type>) {
    return Apply(lhs.value(), rhs.value());
  }
  static bool Apply(num_type lhs, num_type rhs) { return lhs <= rhs; }

  RegOr<operand_type> lhs;
  RegOr<operand_type> rhs;
  Reg result;
};

}  // namespace ir

#endif  // ICARUS_IR_INSTRUCTION_COMPARE_H
