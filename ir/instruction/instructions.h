#ifndef ICARUS_IR_INSTRUCTION_INSTRUCTIONS_H
#define ICARUS_IR_INSTRUCTION_INSTRUCTIONS_H

#include <memory>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ast/scope.h"
#include "base/extend.h"
#include "base/extend/traverse.h"
#include "base/meta.h"
#include "ir/subroutine.h"
#include "ir/instruction/base.h"
#include "ir/instruction/debug.h"
#include "ir/interpreter/architecture.h"
#include "ir/interpreter/execution_context.h"
#include "ir/interpreter/foreign.h"
#include "ir/interpreter/stack_frame.h"
#include "ir/out_params.h"
#include "ir/value/fn.h"
#include "ir/value/reg_or.h"
#include "ir/value/scope.h"
#include "type/array.h"
#include "type/pointer.h"
#include "type/struct.h"
#include "type/type.h"

// This file defines the interface required for IR instructions as well as all
// the common instructions available in the core IR.
namespace ir {

template <typename>
struct CastInstruction;

template <typename ToType, typename FromType>
struct CastInstruction<ToType(FromType)>
    : base::Extend<CastInstruction<ToType(FromType)>>::template With<
          base::BaseTraverseExtension, base::BaseSerializeExtension,
          DebugFormatExtension> {
  using from_type                                = FromType;
  using to_type                                  = ToType;
  static constexpr std::string_view kDebugFormat = "%2$s = cast %1$s";

  ToType Resolve() const {
    if constexpr (base::meta<ToType> == base::meta<ir::Char>) {
      static_assert(sizeof(FromType) == 1, "Invalid cast to ir::Char");
      return static_cast<uint8_t>(value.value());
    }
    FromType from = value.value();
    if constexpr (std::is_integral_v<FromType> or
                  std::is_floating_point_v<FromType>) {
      return from;
    } else {
      return from.template as_type<ToType>();
    }
  }

  RegOr<FromType> value;
  Reg result;
};

struct PtrDiffInstruction
    : base::Extend<PtrDiffInstruction>::With<base::BaseSerializeExtension,
                                             DebugFormatExtension> {
  static constexpr std::string_view kDebugFormat = "%3$s = ptrdiff %1$s %2$s";

  void Apply(interpreter::ExecutionContext& ctx) const {
    ctx.current_frame().set(
        result, (ctx.resolve(lhs) - ctx.resolve(rhs)) /
                    pointee_type.bytes(interpreter::kArchitecture).value());
  }

  friend void BaseTraverse(Inliner& inl, PtrDiffInstruction& inst) {
    base::Traverse(inl, inst.lhs, inst.rhs, inst.result);
  }

  RegOr<addr_t> lhs;
  RegOr<addr_t> rhs;
  type::Type pointee_type;
  Reg result;
};

struct NotInstruction
    : base::Extend<NotInstruction>::With<base::BaseTraverseExtension,
                                         base::BaseSerializeExtension,
                                         DebugFormatExtension> {
  static constexpr std::string_view kDebugFormat = "%2$s = not %1$s";

  bool Resolve() const { return Apply(operand.value()); }
  static bool Apply(bool operand) { return not operand; }

  RegOr<bool> operand;
  Reg result;
};

// TODO Morph this into interpreter break-point instructions.
struct DebugIrInstruction
    : base::Extend<DebugIrInstruction>::With<base::BaseTraverseExtension,
                                             base::BaseSerializeExtension,
                                             DebugFormatExtension> {
  static constexpr std::string_view kDebugFormat = "debug-ir(%s)";

  void Apply(interpreter::ExecutionContext&) const { std::cerr << *fn; }
  Subroutine const* fn;
};

struct InitInstruction
    : base::Extend<InitInstruction>::With<base::BaseSerializeExtension,
                                          DebugFormatExtension> {
  static constexpr std::string_view kDebugFormat = "init %2$s";

  interpreter::StackFrame Apply(interpreter::ExecutionContext& ctx) const {
    if (auto* s = type.if_as<type::Struct>()) {
      ir::Fn f = *s->init_;
      interpreter::StackFrame frame(f.native(), ctx.stack());
      frame.set(ir::Reg::Arg(0), ctx.resolve<ir::addr_t>(reg));
      return frame;

    } else if (auto* a = type.if_as<type::Array>()) {
      ir::Fn f = a->Initializer();
      interpreter::StackFrame frame(f.native(), ctx.stack());
      frame.set(ir::Reg::Arg(0), ctx.resolve<ir::addr_t>(reg));
      return frame;

    } else {
      NOT_YET();
    }
  }

  friend void BaseTraverse(Inliner& inl, InitInstruction& inst) {
    inl(inst.reg);
  }

  type::Type type;
  Reg reg;
};

struct DestroyInstruction
    : base::Extend<DestroyInstruction>::With<base::BaseSerializeExtension,
                                             DebugFormatExtension> {
  static constexpr std::string_view kDebugFormat = "destroy %2$s";

  interpreter::StackFrame Apply(interpreter::ExecutionContext& ctx) const {
    interpreter::StackFrame frame(function(), ctx.stack());
    frame.set(ir::Reg::Arg(0), ctx.resolve(addr));
    return frame;
  }

  friend void BaseTraverse(Inliner& inl, DestroyInstruction& inst) {
    inl(inst.addr);
  }

  type::Type type;
  RegOr<addr_t> addr;

 private:
  ir::Fn function() const {
    if (auto* s = type.if_as<type::Struct>()) {
      ASSERT(s->dtor_.has_value() == true);
      return *s->dtor_;
    } else if (auto* a = type.if_as<type::Array>()) {
      return a->Destructor();
    } else {
      NOT_YET();
    }
  }
};

struct CopyInstruction
    : base::Extend<CopyInstruction>::With<base::BaseSerializeExtension,
                                          DebugFormatExtension> {
  static constexpr std::string_view kDebugFormat = "copy %2$s to %3$s";

  interpreter::StackFrame Apply(interpreter::ExecutionContext& ctx) const {
    interpreter::StackFrame frame(function(), ctx.stack());
    frame.set(ir::Reg::Arg(0), ctx.resolve<ir::addr_t>(to));
    frame.set(ir::Reg::Arg(1), ctx.resolve<ir::addr_t>(from));
    return frame;
  }

  friend void BaseTraverse(Inliner& inl, CopyInstruction& inst) {
    base::Traverse(inl, inst.from, inst.to);
  }

  type::Type type;
  ir::RegOr<ir::addr_t> from;
  ir::RegOr<ir::addr_t> to;

 private:
  ir::Fn function() const {
    if (auto* s = type.if_as<type::Struct>()) {
      ASSERT(s->completeness() == type::Completeness::Complete);
      return *ASSERT_NOT_NULL(s->CopyAssignment(s));
    } else if (auto* a = type.if_as<type::Array>()) {
      return a->CopyAssign();
    } else {
      NOT_YET();
    }
  }
};

struct CopyInitInstruction
    : base::Extend<CopyInitInstruction>::With<base::BaseSerializeExtension,
                                              DebugFormatExtension> {
  static constexpr std::string_view kDebugFormat = "copy-init %2$s to %3$s";

  interpreter::StackFrame Apply(interpreter::ExecutionContext& ctx) const {
    interpreter::StackFrame frame(function(), ctx.stack());
    frame.set(ir::Reg::Arg(0), ctx.resolve<ir::addr_t>(from));
    frame.set(ir::Reg::Out(0), ctx.resolve<ir::addr_t>(to));
    return frame;
  }

  friend void BaseTraverse(Inliner& inl, CopyInitInstruction& inst) {
    base::Traverse(inl, inst.from, inst.to);
  }

  type::Type type;
  ir::RegOr<ir::addr_t> from;
  ir::RegOr<ir::addr_t> to;

 private:
  ir::Fn function() const {
    if (auto* s = type.if_as<type::Struct>()) {
      ASSERT(s->completeness() == type::Completeness::Complete);
      return *ASSERT_NOT_NULL(s->CopyInit(s));
    } else if (auto* a = type.if_as<type::Array>()) {
      return a->CopyInit();
    } else {
      NOT_YET();
    }
  }
};

struct MoveInstruction
    : base::Extend<MoveInstruction>::With<base::BaseSerializeExtension,
                                          DebugFormatExtension> {
  static constexpr std::string_view kDebugFormat = "move %2$s to %3$s";

  interpreter::StackFrame Apply(interpreter::ExecutionContext& ctx) const {
    interpreter::StackFrame frame(function(), ctx.stack());
    frame.set(ir::Reg::Arg(0), ctx.resolve<ir::addr_t>(to));
    frame.set(ir::Reg::Arg(1), ctx.resolve<ir::addr_t>(from));
    return frame;
  }

  friend void BaseTraverse(Inliner& inl, MoveInstruction& inst) {
    base::Traverse(inl, inst.from, inst.to);
  }

  type::Type type;
  ir::RegOr<ir::addr_t> from;
  ir::RegOr<ir::addr_t> to;

 private:
  ir::Fn function() const {
    if (auto* s = type.if_as<type::Struct>()) {
      ASSERT(s->completeness() == type::Completeness::Complete);
      return *ASSERT_NOT_NULL(s->MoveAssignment(s));
    } else if (auto* a = type.if_as<type::Array>()) {
      return a->MoveAssign();
    } else {
      NOT_YET();
    }
  }
};

struct MoveInitInstruction
    : base::Extend<MoveInitInstruction>::With<base::BaseSerializeExtension,
                                              DebugFormatExtension> {
  static constexpr std::string_view kDebugFormat = "move-init %2$s to %3$s";

  interpreter::StackFrame Apply(interpreter::ExecutionContext& ctx) const {
    interpreter::StackFrame frame(function(), ctx.stack());
    frame.set(ir::Reg::Arg(0), ctx.resolve<ir::addr_t>(from));
    frame.set(ir::Reg::Out(0), ctx.resolve<ir::addr_t>(to));
    return frame;
  }

  friend void BaseTraverse(Inliner& inl, MoveInitInstruction& inst) {
    base::Traverse(inl, inst.from, inst.to);
  }

  type::Type type;
  ir::RegOr<ir::addr_t> from;
  ir::RegOr<ir::addr_t> to;

 private:
  ir::Fn function() const {
    if (auto* s = type.if_as<type::Struct>()) {
      ASSERT(s->completeness() == type::Completeness::Complete);
      return *ASSERT_NOT_NULL(s->MoveInit(s));
    } else if (auto* a = type.if_as<type::Array>()) {
      return a->MoveInit();
    } else {
      NOT_YET();
    }
  }
};

[[noreturn]] inline void FatalInterpreterError(std::string_view err_msg) {
  // TODO: Add a diagnostic explaining the failure.
  absl::FPrintF(stderr,
                R"(
  ----------------------------------------
  Fatal interpreter failure:
    %s
  ----------------------------------------)"
                "\n",
                err_msg);
  std::terminate();
}

struct LoadDataSymbolInstruction
    : base::Extend<LoadDataSymbolInstruction>::With<
          base::BaseSerializeExtension, DebugFormatExtension> {
  static constexpr std::string_view kDebugFormat = "%2$s = load-symbol %1$s";

  void Apply(interpreter::ExecutionContext& ctx) const {
    absl::StatusOr<void*> sym = interpreter::LoadDataSymbol(name);
    if (not sym.ok()) { FatalInterpreterError(sym.status().message()); }
    ctx.current_frame().set(result, ir::Addr(*sym));
  }

  friend void BaseTraverse(Inliner& inl, LoadDataSymbolInstruction& inst) {
    inl(inst.result);
  }

  std::string name;
  Reg result;
};

struct StructIndexInstruction
    : base::Extend<StructIndexInstruction>::With<base::BaseTraverseExtension,
                                                 base::BaseSerializeExtension,
                                                 DebugFormatExtension> {
  static constexpr std::string_view kDebugFormat =
      "%4$s = index %2$s of %1$s (struct %3$s)";

  addr_t Resolve() const {
    return addr.value() +
           struct_type->offset(index.value(), interpreter::kArchitecture)
               .value();
  }

  RegOr<addr_t> addr;
  RegOr<int64_t> index;
  ::type::Struct const* struct_type;
  Reg result;
};

struct PtrIncrInstruction
    : base::Extend<PtrIncrInstruction>::With<base::BaseTraverseExtension,
                                             base::BaseSerializeExtension,
                                             DebugFormatExtension> {
  static constexpr std::string_view kDebugFormat =
      "%4$s = index %2$s of %1$s (pointer %3$s)";

  addr_t Resolve() const {
    return addr.value() +
           (core::FwdAlign(
                ptr->pointee().bytes(interpreter::kArchitecture),
                ptr->pointee().alignment(interpreter::kArchitecture)) *
            index.value())
               .value();
  }

  RegOr<addr_t> addr;
  RegOr<int64_t> index;
  ::type::Pointer const* ptr;
  Reg result;
};

struct AndInstruction
    : base::Extend<AndInstruction>::With<base::BaseTraverseExtension,
                                         base::BaseSerializeExtension,
                                         DebugFormatExtension> {
  static constexpr std::string_view kDebugFormat = "%3$s = and %1$s %2$s";

  bool Resolve() const { return Apply(lhs.value(), rhs.value()); }
  static bool Apply(bool lhs, bool rhs) { return lhs and rhs; }

  RegOr<bool> lhs;
  RegOr<bool> rhs;
  Reg result;
};

}  // namespace ir

#endif  // ICARUS_IR_INSTRUCTION_INSTRUCTIONS_H
