#ifndef ICARUS_COMPILER_IR_BUILDER_H
#define ICARUS_COMPILER_IR_BUILDER_H

#include <vector>

#include "absl/types/span.h"
#include "base/debug.h"
#include "base/meta.h"
#include "base/scope.h"
#include "base/untyped_buffer.h"
#include "ir/blocks/basic.h"
#include "ir/blocks/group.h"
#include "ir/instruction/arithmetic.h"
#include "ir/instruction/compare.h"
#include "ir/instruction/core.h"
#include "ir/instruction/instructions.h"
#include "ir/out_params.h"
#include "ir/scope_state.h"
#include "ir/value/addr.h"
#include "ir/value/char.h"
#include "ir/value/module_id.h"
#include "ir/value/reg.h"
#include "ir/value/scope.h"
#include "type/array.h"
#include "type/enum.h"
#include "type/flags.h"
#include "type/function.h"
#include "type/interface/interface.h"
#include "type/opaque.h"
#include "type/pointer.h"
#include "type/primitive.h"
#include "type/scope.h"
#include "type/slice.h"
#include "type/struct.h"
#include "type/type.h"
#include "type/typed_value.h"

namespace compiler {

// TODO: Remove as much as possible from IrBuilder. I'm not exactly sure what
// I want to do with this, but we should really limit it only to core
// instructions and/or instructions that can only be optimized in the streaming
// approach by carrying around extra state (like loads/store-caching).

struct IrBuilder {
  explicit IrBuilder(ir::internal::BlockGroupBase* group,
                     ast::FnScope const* fn_scope)
      : group_(ASSERT_NOT_NULL(group)) {
    CurrentBlock() = group_->entry();
  }

  ir::BasicBlock* EmitDestructionPath(ast::Scope const* from,
                                      ast::Scope const* to);

  ir::Reg Reserve() { return CurrentGroup()->Reserve(); }

  ir::internal::BlockGroupBase*& CurrentGroup() { return group_; }
  ir::BasicBlock*& CurrentBlock() { return current_.block_; }

  template <typename T>
  struct reduced_type {
    using type = T;
  };
  template <typename T>
  struct reduced_type<ir::RegOr<T>> {
    using type = T;
  };

  template <typename T>
  using reduced_type_t = typename reduced_type<T>::type;

  // INSTRUCTIONS

  template <typename Lhs, typename Rhs>
  ir::RegOr<bool> Le(Lhs const& lhs, Rhs const& rhs) {
    using type = reduced_type_t<Lhs>;
    if constexpr (base::meta<Lhs>.template is_a<ir::RegOr>() and
                  base::meta<Rhs>.template is_a<ir::RegOr>()) {
      if (not lhs.is_reg() and not rhs.is_reg()) {
        return ir::LeInstruction<type>::Apply(lhs.value(), rhs.value());
      }

      return CurrentBlock()->Append(ir::LeInstruction<type>{
          .lhs = lhs, .rhs = rhs, .result = CurrentGroup()->Reserve()});
    } else {
      return Le(ir::RegOr<type>(lhs), ir::RegOr<type>(rhs));
    }
  }

  // Comparison
  template <typename Lhs, typename Rhs>
  ir::RegOr<bool> Eq(Lhs const& lhs, Rhs const& rhs) {
    using type = reduced_type_t<Lhs>;
    if constexpr (base::meta<type> == base::meta<bool>) {
      return EqBool(lhs, rhs);
    } else if constexpr (base::meta<Lhs>.template is_a<ir::RegOr>() and
                         base::meta<Rhs>.template is_a<ir::RegOr>()) {
      if (not lhs.is_reg() and not rhs.is_reg()) {
        return ir::EqInstruction<type>::Apply(lhs.value(), rhs.value());
      }
      return CurrentBlock()->Append(ir::EqInstruction<type>{
          .lhs = lhs, .rhs = rhs, .result = CurrentGroup()->Reserve()});
    } else {
      return Eq(ir::RegOr<type>(lhs), ir::RegOr<type>(rhs));
    }
  }

  template <typename Lhs, typename Rhs>
  ir::RegOr<bool> Ne(Lhs const& lhs, Rhs const& rhs) {
    using type = reduced_type_t<Lhs>;
    if constexpr (std::is_same_v<type, bool>) {
      return NeBool(lhs, rhs);
    } else if constexpr (base::meta<Lhs>.template is_a<ir::RegOr>() and
                         base::meta<Rhs>.template is_a<ir::RegOr>()) {
      if (not lhs.is_reg() and not rhs.is_reg()) {
        return ir::NeInstruction<type>::Apply(lhs.value(), rhs.value());
      }
      return CurrentBlock()->Append(ir::NeInstruction<type>{
          .lhs = lhs, .rhs = rhs, .result = CurrentGroup()->Reserve()});
    } else {
      return Ne(ir::RegOr<type>(lhs), ir::RegOr<type>(rhs));
    }
  }

  template <typename T>
  ir::RegOr<T> Neg(ir::RegOr<T> const& val) {
    if (not val.is_reg()) { return ir::NegInstruction<T>::Apply(val.value()); }
    return CurrentBlock()->Append(ir::NegInstruction<reduced_type_t<T>>{
        .operand = val, .result = CurrentGroup()->Reserve()});
  }

  ir::RegOr<bool> Not(ir::RegOr<bool> const& val) {
    if (not val.is_reg()) { return ir::NotInstruction::Apply(val.value()); }
    return CurrentBlock()->Append(ir::NotInstruction{
        .operand = val, .result = CurrentGroup()->Reserve()});
  }

  template <typename ToType>
  ir::RegOr<ToType> CastTo(type::Type t, ir::PartialResultRef const& buffer) {
    if (t == GetType<ToType>() or
        (t.is<type::Pointer>() and base::meta<ToType> == base::meta<ir::addr_t>)) {
      return buffer.get<ToType>();
    }
    if (auto const* p = t.if_as<type::Primitive>()) {
      return p->Apply([&]<typename T>() -> ir::RegOr<ToType> {
        return Cast<T, ToType>(buffer.get<T>());
      });
    } else if (t.is<type::Enum>()) {
      if constexpr (base::meta<ToType> ==
                    base::meta<type::Enum::underlying_type>) {
        return buffer.get<type::Enum::underlying_type>();
      } else {
        return Cast<type::Enum::underlying_type, ToType>(
            buffer.get<type::Enum::underlying_type>());
      }
    } else if (t.is<type::Flags>()) {
      if constexpr (base::meta<ToType> ==
                    base::meta<type::Flags::underlying_type>) {
        return buffer.get<type::Flags::underlying_type>();
      } else {
        return Cast<type::Flags::underlying_type, ToType>(
            buffer.get<type::Flags::underlying_type>());
      }
    } else {
      UNREACHABLE(t, "cannot cast to", GetType<ToType>());
    }
  }

  template <typename FromType, typename ToType>
  ir::RegOr<ToType> Cast(ir::RegOr<FromType> r) {
    if constexpr (base::meta<ToType> == base::meta<FromType>) {
      return r;
    } else if constexpr ((base::meta<FromType> == base::meta<ir::Integer> and
                          std::is_arithmetic_v<ToType>) or
                         base::meta<FromType>.template converts_to<ToType>()) {
      if (r.is_reg()) {
        return CurrentBlock()->Append(ir::CastInstruction<ToType(FromType)>{
            .value = r.reg(), .result = CurrentGroup()->Reserve()});
      } else {
        if constexpr (base::meta<FromType> == base::meta<ir::Integer>) {
          return static_cast<ToType>(r.value().value());
        } else {
          return static_cast<ToType>(r.value());
        }
      }
    } else {
      UNREACHABLE(GetType<FromType>(), "cannot cast to", GetType<ToType>());
    }
  }

  // Phi instruction. Takes a span of basic blocks and a span of (registers or)
  // values. As a precondition, the number of blocks must be equal to the number
  // of values. This instruction evaluates to the value `values[i]` if the
  // previous block was `blocks[i]`.
  //
  // In the first overload, the resulting value is assigned to `r`. In the
  // second overload, a register is constructed to represent the value.
  template <typename T>
  void Phi(ir::Reg r, std::vector<ir::BasicBlock const*> blocks,
           std::vector<ir::RegOr<T>> values) {
    ASSERT(blocks.size() == values.size());
    ir::PhiInstruction<T> inst(std::move(blocks), std::move(values));
    inst.result = r;
  }

  template <typename T>
  ir::RegOr<T> Phi(std::vector<ir::BasicBlock const*> blocks,
                   std::vector<ir::RegOr<T>> values) {
    if (values.size() == 1u) { return values[0]; }
    ir::PhiInstruction<T> inst(std::move(blocks), std::move(values));
    auto result = inst.result = CurrentGroup()->Reserve();
    CurrentBlock()->Append(std::move(inst));
    return result;
  }

  ir::Reg PtrFix(ir::RegOr<ir::addr_t> addr, type::Type desired_type) {
    // TODO must this be a register if it's loaded?
    if (desired_type.get()->is_big()) { return addr.reg(); }
    ir::PartialResultBuffer buffer;
    Load(addr, desired_type, buffer);
    return buffer.get<ir::Reg>(0);
  }

  template <typename T>
  ir::RegOr<T> Load(ir::RegOr<ir::addr_t> addr, type::Type t = GetType<T>()) {
    ASSERT(addr != ir::Null());
    auto& blk = *CurrentBlock();

    auto [slot, inserted] = blk.load_store_cache().slot<T>(addr);
    if (not inserted) { return slot; }

    ir::LoadInstruction inst{.type = t, .addr = addr};
    auto result = inst.result = CurrentGroup()->Reserve();

    slot = result;

    blk.Append(std::move(inst));
    return result;
  }

  void Load(ir::RegOr<ir::addr_t> r, type::Type t, ir::PartialResultBuffer& out) {
    LOG("Load", "Calling Load(%s, %s)", r, t.to_string());
    ASSERT(r != ir::Null());
    if (t.is<type::Function>()) {
      out.append(Load<ir::Fn>(r, t));
    } else if (t.is<type::Pointer>()) {
      out.append(Load<ir::addr_t>(r, t));
    } else if (t.is<type::Enum>()) {
      out.append(Load<type::Enum::underlying_type>(r, t));
    } else if (t.is<type::Flags>()) {
      out.append(Load<type::Flags::underlying_type>(r, t));
    } else {
      t.as<type::Primitive>().Apply(
          [&]<typename T>() { return out.append(Load<T>(r, t)); });
    }
  }

  template <typename T>
  void Store(T r, ir::RegOr<ir::addr_t> addr) {
    if constexpr (base::meta<T>.template is_a<ir::RegOr>()) {
      auto& blk = *CurrentBlock();
      blk.load_store_cache().clear<typename T::type>();
      blk.Append(
          ir::StoreInstruction<typename T::type>{.value = r, .location = addr});
    } else {
      Store(ir::RegOr<T>(r), addr);
    }
  }

  ir::Reg Index(type::Pointer const* t, ir::RegOr<ir::addr_t> addr,
                ir::RegOr<int64_t> offset) {
    type::Type pointee = t->pointee();
    type::Type data_type;
    if (auto const* a = pointee.if_as<type::Array>()) {
      data_type = a->data_type();
    } else if (auto const* s = pointee.if_as<type::Slice>()) {
      data_type = s->data_type();
    } else {
      UNREACHABLE(t->to_string());
    }

    return PtrIncr(addr, offset, type::Ptr(data_type));
  }

  // Emits a function-call instruction, calling `fn` of type `f` with the given
  // `arguments` and output parameters. If output parameters are not present,
  // the function must return nothing.
  void Call(ir::RegOr<ir::Fn> const& fn, type::Function const* f,
            ir::PartialResultBuffer args, ir::OutParams outs);

  // Jump instructions must be the last instruction in a basic block. They
  // handle control-flow, indicating which basic block control should be
  // transferred to next.
  //
  // `UncondJump`:   Transfers control to `block`.
  // `CondJump`:     Transfers control to one of two blocks depending on a
  //                 run-time boolean value.
  // `ReturnJump`:   Transfers control back to the calling function.
  void UncondJump(ir::BasicBlock* block);
  void CondJump(ir::RegOr<bool> cond, ir::BasicBlock* true_block,
                ir::BasicBlock* false_block);
  void ReturnJump();
  void BlockJump(ir::Block b, ir::BasicBlock* after);

  // Special members function instructions. Calling these typically calls
  // builtin functions (or, in the case of primitive types, do nothing).
  void Move(type::Typed<ir::RegOr<ir::addr_t>> to, type::Typed<ir::Reg> from);
  void Copy(type::Typed<ir::RegOr<ir::addr_t>> to, type::Typed<ir::Reg> from);

  // Data structure access commands. For structs, `Fields` takes an
  // address of the data structure and returns the address of the particular
  // field requested. For variants, `VariantType` computes the location where
  // the type is stored and `VariantValue` accesses the location where the
  // value is stored.
  type::Typed<ir::Reg> FieldRef(ir::RegOr<ir::addr_t> r, type::Struct const* t,
                                int64_t n);

  type::Type FieldValue(ir::RegOr<ir::addr_t> r, type::Struct const* t, int64_t n,
                        ir::PartialResultBuffer& out) {
    auto typed_reg = FieldRef(r, t, n);
    out.append(PtrFix(*typed_reg, typed_reg.type()));
    return typed_reg.type();
  }
  ir::Reg PtrIncr(ir::RegOr<ir::addr_t> ptr, ir::RegOr<int64_t> inc,
                  type::Pointer const* t);

  ir::Reg Alloca(type::Type t);

  void DebugIr() {
    CurrentBlock()->Append(ir::DebugIrInstruction{.fn = CurrentGroup()});
  }

 private:
  template <typename T>
  static type::Type GetType() {
    if constexpr (base::meta<T> == base::meta<bool>) {
      return type::Bool;
    } else if constexpr (base::meta<T> == base::meta<ir::Integer>) {
      return type::Integer;
    } else if constexpr (base::meta<T> == base::meta<ir::Char>) {
      return type::Char;
    } else if constexpr (base::meta<T> == base::meta<ir::memory_t>) {
      return type::Byte;
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
    } else if constexpr (base::meta<T> == base::meta<type::Type>) {
      return type::Type_;
    } else if constexpr (base::meta<T> == base::meta<ir::Scope>) {
      return type::Scp({});
    } else if constexpr (base::meta<T> == base::meta<ir::ModuleId>) {
      return type::Module;
    } else if constexpr (base::meta<T> == base::meta<interface::Interface>) {
      return type::Interface;
    } else if constexpr (std::is_pointer_v<T>) {
      return type::Ptr(GetType<std::decay_t<decltype(*std::declval<T>())>>());
    } else {
      UNREACHABLE(typeid(T).name());
    }
  }

  ir::RegOr<bool> EqBool(ir::RegOr<bool> const& lhs,
                         ir::RegOr<bool> const& rhs) {
    if (not lhs.is_reg()) { return lhs.value() ? rhs : Not(rhs); }
    if (not rhs.is_reg()) { return rhs.value() ? lhs : Not(lhs); }
    return CurrentBlock()->Append(ir::EqInstruction<bool>{
        .lhs = lhs, .rhs = rhs, .result = CurrentGroup()->Reserve()});
  }

  ir::RegOr<bool> NeBool(ir::RegOr<bool> const& lhs,
                         ir::RegOr<bool> const& rhs) {
    if (not lhs.is_reg()) { return lhs.value() ? Not(rhs) : rhs; }
    if (not rhs.is_reg()) { return rhs.value() ? Not(lhs) : lhs; }
    return CurrentBlock()->Append(ir::NeInstruction<bool>{
        .lhs = lhs, .rhs = rhs, .result = CurrentGroup()->Reserve()});
  }

  struct State {
    ir::BasicBlock* block_;
  } current_;

  ir::internal::BlockGroupBase* group_;
};

ir::Reg RegisterReferencing(IrBuilder& builder, type::Type t,
                            ir::PartialResultRef const& value);

}  // namespace compiler

#endif  // ICARUS_COMPILER_IR_BUILDER_H
