#include "backend/function.h"

#include "absl/container/flat_hash_map.h"
#include "backend/type.h"
#include "base/log.h"
#include "base/meta.h"
#include "ir/instruction/instructions.h"
#include "llvm/IR/Function.h"

namespace backend {
namespace {

// TODO: Mapping isn't really the right name because it holds the builder too.
struct IrToLlvmMapping {
  llvm::IRBuilder<> &builder;
  absl::flat_hash_map<ir::BasicBlock const *, llvm::BasicBlock *> blocks;
  absl::flat_hash_map<ir::Reg, llvm::Value *> registers;
  absl::flat_hash_map<ir::CompiledFn const *, llvm::Function *> const &fn_map;

  template <typename T>
  llvm::Value *Lookup(ir::RegOr<T> val, llvm::IRBuilder<> &builder) {
    if (val.is_reg()) {
      if (val.reg().is_arg()) {
        // TODO: We need to handle offsets here because the numbers don't
        // exaclty line up. Constant arguments in Icarus functions do not get
        // propagated to LLVM.

        // TODO: Use getArg when you upgrade to llvm 11.0
        return builder.GetInsertBlock()->getParent()->arg_begin() +
               val.reg().arg_value();
      }
      auto iter = registers.find(val.reg());
      if (iter == registers.end()) { return nullptr; }
      return iter->second;
    } else {
      if constexpr (std::is_integral_v<T>) {
        return llvm::Constant::getIntegerValue(
            LlvmType<T>(builder.getContext()),
            llvm::APInt(sizeof(T) * CHAR_BIT,
                        static_cast<uint64_t>(val.value()),
                        std::is_signed_v<T>));
      } else if constexpr (base::meta<T> == base::meta<ir::Fn>) {
        switch (val.value().kind()) {
          case ir::Fn::Kind::Native: {
            return fn_map.at(val.value().native().get());
          } break;
          default: NOT_YET();
        }
      } else {
        NOT_YET(typeid(T).name());
      }
    }
  }

  void insert(ir::Reg reg, llvm::Value *value) {
    registers.emplace(reg, value);
  }
};

template <typename Inst>
std::optional<std::pair<llvm::Value *, llvm::Value *>>
GatherBinaryInstructionOperands(Inst const &inst, IrToLlvmMapping &to_llvm) {
  ASSIGN_OR(return std::nullopt, auto &lhs_value,
                   to_llvm.Lookup(inst.lhs, to_llvm.builder));
  ASSIGN_OR(return std::nullopt, auto &rhs_value,
                   to_llvm.Lookup(inst.rhs, to_llvm.builder));
  return std::pair(&lhs_value, &rhs_value);
}

void TryEmitLlvmBasicBlock(ir::BasicBlock const *block,
                           absl::Span<ir::Inst const> &instructions,
                           IrToLlvmMapping &to_llvm) {
  to_llvm.builder.SetInsertPoint(to_llvm.blocks.at(block));
  for (auto const &instruction : block->instructions()) {
    LOG("llvm-inst", "%s", instruction.to_string());
    if (auto const *inst = instruction.if_as<ir::AddInstruction<int8_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNSWAdd(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::AddInstruction<uint8_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNUWAdd(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::AddInstruction<int16_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNSWAdd(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::AddInstruction<uint16_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNUWAdd(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::AddInstruction<int32_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNSWAdd(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::AddInstruction<uint32_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNUWAdd(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::AddInstruction<int64_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNSWAdd(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::AddInstruction<uint64_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNUWAdd(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::AddInstruction<float>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateAdd(operands.first,
                                                             operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::AddInstruction<double>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateAdd(operands.first,
                                                             operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::SubInstruction<int8_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNSWSub(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::SubInstruction<uint8_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNUWSub(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::SubInstruction<int16_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNSWSub(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::SubInstruction<uint16_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNUWSub(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::SubInstruction<int32_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNSWSub(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::SubInstruction<uint32_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNUWSub(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::SubInstruction<int64_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNSWSub(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::SubInstruction<uint64_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNUWSub(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::SubInstruction<float>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateSub(operands.first,
                                                             operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::SubInstruction<double>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateSub(operands.first,
                                                             operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::MulInstruction<int8_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNSWMul(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::MulInstruction<uint8_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNUWMul(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::MulInstruction<int16_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNSWMul(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::MulInstruction<uint16_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNUWMul(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::MulInstruction<int32_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNSWMul(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::MulInstruction<uint32_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNUWMul(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::MulInstruction<int64_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNSWMul(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::MulInstruction<uint64_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateNUWMul(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::MulInstruction<float>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateMul(operands.first,
                                                             operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::MulInstruction<double>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateMul(operands.first,
                                                             operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::DivInstruction<int8_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateSDiv(operands.first,
                                                              operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::DivInstruction<uint8_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateUDiv(operands.first,
                                                              operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::DivInstruction<int16_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateSDiv(operands.first,
                                                              operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::DivInstruction<uint16_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateUDiv(operands.first,
                                                              operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::DivInstruction<int32_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateSDiv(operands.first,
                                                              operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::DivInstruction<uint32_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateUDiv(operands.first,
                                                              operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::DivInstruction<int64_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateSDiv(operands.first,
                                                              operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::DivInstruction<uint64_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateUDiv(operands.first,
                                                              operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::DivInstruction<float>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateFDiv(operands.first,
                                                              operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::DivInstruction<double>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateFDiv(operands.first,
                                                              operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::ModInstruction<int8_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateSRem(operands.first,
                                                              operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::ModInstruction<uint8_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateURem(operands.first,
                                                              operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::ModInstruction<int16_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateSRem(operands.first,
                                                              operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::ModInstruction<uint16_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateURem(operands.first,
                                                              operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::ModInstruction<int32_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateSRem(operands.first,
                                                              operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::ModInstruction<uint32_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateURem(operands.first,
                                                              operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::ModInstruction<int64_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateSRem(operands.first,
                                                              operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::ModInstruction<uint64_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateURem(operands.first,
                                                              operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LtInstruction<int8_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateICmpSLT(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LtInstruction<uint8_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateICmpULT(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LtInstruction<int16_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateICmpSLT(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LtInstruction<uint16_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateICmpULT(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LtInstruction<int32_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateICmpSLT(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LtInstruction<uint32_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateICmpULT(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LtInstruction<int64_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateICmpSLT(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LtInstruction<uint64_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateICmpULT(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LtInstruction<float>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateFCmpULT(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LtInstruction<double>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateFCmpULT(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LeInstruction<int8_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateICmpSLE(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LeInstruction<uint8_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateICmpULE(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LeInstruction<int16_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateICmpSLE(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LeInstruction<uint16_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateICmpULE(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LeInstruction<int32_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateICmpSLE(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LeInstruction<uint32_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateICmpULE(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LeInstruction<int64_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateICmpSLE(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LeInstruction<uint64_t>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateICmpULE(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LeInstruction<float>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateFCmpULE(
                                       operands.first, operands.second));
    } else if (auto const *inst =
                   instruction.if_as<ir::LeInstruction<double>>()) {
      ASSIGN_OR(return, auto operands,
                      GatherBinaryInstructionOperands(*inst, to_llvm));
      to_llvm.insert(inst->result, to_llvm.builder.CreateFCmpULE(
                                       operands.first, operands.second));
    } else if (auto const *inst = instruction.if_as<ir::PtrIncrInstruction>()) {
      to_llvm.builder.CreateGEP(
          ToLlvmType(inst->ptr, to_llvm.builder.getContext()),
          to_llvm.Lookup(inst->addr, to_llvm.builder),
          to_llvm.Lookup(inst->index, to_llvm.builder));
    } else if (auto const *inst =
                   instruction.if_as<ir::SetReturnInstruction<bool>>()) {
      if (inst->index == 0) {
        to_llvm.builder.CreateRet(to_llvm.Lookup(inst->value, to_llvm.builder));
      } else {
        NOT_YET("Not yet supporting multiple returns: index = ", inst->index);
      }
    } else if (auto const *inst =
                   instruction.if_as<ir::SetReturnInstruction<int8_t>>()) {
      if (inst->index == 0) {
        to_llvm.builder.CreateRet(to_llvm.Lookup(inst->value, to_llvm.builder));
      } else {
        NOT_YET("Not yet supporting multiple returns: index = ", inst->index);
      }
    } else if (auto const *inst =
                   instruction.if_as<ir::SetReturnInstruction<int8_t>>()) {
      if (inst->index == 0) {
        to_llvm.builder.CreateRet(to_llvm.Lookup(inst->value, to_llvm.builder));
      } else {
        NOT_YET("Not yet supporting multiple returns: index = ", inst->index);
      }
    } else if (auto const *inst =
                   instruction.if_as<ir::SetReturnInstruction<int16_t>>()) {
      if (inst->index == 0) {
        to_llvm.builder.CreateRet(to_llvm.Lookup(inst->value, to_llvm.builder));
      } else {
        NOT_YET("Not yet supporting multiple returns: index = ", inst->index);
      }
    } else if (auto const *inst =
                   instruction.if_as<ir::SetReturnInstruction<uint16_t>>()) {
      if (inst->index == 0) {
        to_llvm.builder.CreateRet(to_llvm.Lookup(inst->value, to_llvm.builder));
      } else {
        NOT_YET("Not yet supporting multiple returns: index = ", inst->index);
      }
    } else if (auto const *inst =
                   instruction.if_as<ir::SetReturnInstruction<int32_t>>()) {
      if (inst->index == 0) {
        to_llvm.builder.CreateRet(to_llvm.Lookup(inst->value, to_llvm.builder));
      } else {
        NOT_YET("Not yet supporting multiple returns: index = ", inst->index);
      }
    } else if (auto const *inst =
                   instruction.if_as<ir::SetReturnInstruction<uint32_t>>()) {
      if (inst->index == 0) {
        to_llvm.builder.CreateRet(to_llvm.Lookup(inst->value, to_llvm.builder));
      } else {
        NOT_YET("Not yet supporting multiple returns: index = ", inst->index);
      }
    } else if (auto const *inst =
                   instruction.if_as<ir::SetReturnInstruction<int64_t>>()) {
      if (inst->index == 0) {
        to_llvm.builder.CreateRet(to_llvm.Lookup(inst->value, to_llvm.builder));
      } else {
        NOT_YET("Not yet supporting multiple returns: index = ", inst->index);
      }
    } else if (auto const *inst =
                   instruction.if_as<ir::SetReturnInstruction<uint64_t>>()) {
      if (inst->index == 0) {
        to_llvm.builder.CreateRet(to_llvm.Lookup(inst->value, to_llvm.builder));
      } else {
        NOT_YET("Not yet supporting multiple returns: index = ", inst->index);
      }
    } else if (auto const *inst =
                   instruction.if_as<ir::SetReturnInstruction<float>>()) {
      if (inst->index == 0) {
        to_llvm.builder.CreateRet(to_llvm.Lookup(inst->value, to_llvm.builder));
      } else {
        NOT_YET("Not yet supporting multiple returns: index = ", inst->index);
      }
    } else if (auto const *inst =
                   instruction.if_as<ir::SetReturnInstruction<double>>()) {
      if (inst->index == 0) {
        to_llvm.builder.CreateRet(to_llvm.Lookup(inst->value, to_llvm.builder));
      } else {
        NOT_YET("Not yet supporting multiple returns: index = ", inst->index);
      }
    } else if (auto const *inst = instruction.if_as<ir::CallInstruction>()) {
      // TODO: support multiple outputs
      if (inst->outputs().size() > 1) { NOT_YET(); }
      std::vector<llvm::Value *> args;
      args.reserve(inst->arguments().size());
      for (auto const &arg : inst->arguments()) {
        arg.apply([&](auto v) {
          using type = std::decay_t<decltype(v)>;
          if constexpr (base::meta<type> == base::meta<ir::Reg>) {
            NOT_YET();
          } else {
            args.push_back(to_llvm.Lookup<type>(v, to_llvm.builder));
          }
        });
      }
      llvm::Value *result = to_llvm.builder.CreateCall(
          llvm::cast<llvm::FunctionType>(
              ToLlvmType(inst->func_type(), to_llvm.builder.getContext())),
          to_llvm.Lookup(inst->func(), to_llvm.builder), args);
      if (inst->outputs().size() == 1) {
        to_llvm.insert(inst->outputs()[0], result);
      }
    } else if (auto const *inst = instruction.if_as<ir::LoadInstruction>()) {
      to_llvm.insert(inst->result,
                     to_llvm.builder.CreateLoad(
                         ToLlvmType(inst->type, to_llvm.builder.getContext()),
                         to_llvm.Lookup(inst->addr, to_llvm.builder)));
    } else if (auto const *inst =
                   instruction.if_as<ir::StoreInstruction<bool>>()) {
      to_llvm.builder.CreateStore(
          to_llvm.Lookup(inst->value, to_llvm.builder),
          to_llvm.Lookup(inst->location, to_llvm.builder));
    } else if (auto const *inst =
                   instruction.if_as<ir::StoreInstruction<ir::Char>>()) {
      to_llvm.builder.CreateStore(
          to_llvm.Lookup(inst->value, to_llvm.builder),
          to_llvm.Lookup(inst->location, to_llvm.builder));
    } else if (auto const *inst =
                   instruction.if_as<ir::StoreInstruction<int8_t>>()) {
      to_llvm.builder.CreateStore(
          to_llvm.Lookup(inst->value, to_llvm.builder),
          to_llvm.Lookup(inst->location, to_llvm.builder));
    } else if (auto const *inst =
                   instruction.if_as<ir::StoreInstruction<uint8_t>>()) {
      to_llvm.builder.CreateStore(
          to_llvm.Lookup(inst->value, to_llvm.builder),
          to_llvm.Lookup(inst->location, to_llvm.builder));
    } else if (auto const *inst =
                   instruction.if_as<ir::StoreInstruction<int16_t>>()) {
      to_llvm.builder.CreateStore(
          to_llvm.Lookup(inst->value, to_llvm.builder),
          to_llvm.Lookup(inst->location, to_llvm.builder));
    } else if (auto const *inst =
                   instruction.if_as<ir::StoreInstruction<uint16_t>>()) {
      to_llvm.builder.CreateStore(
          to_llvm.Lookup(inst->value, to_llvm.builder),
          to_llvm.Lookup(inst->location, to_llvm.builder));
    } else if (auto const *inst =
                   instruction.if_as<ir::StoreInstruction<int32_t>>()) {
      to_llvm.builder.CreateStore(
          to_llvm.Lookup(inst->value, to_llvm.builder),
          to_llvm.Lookup(inst->location, to_llvm.builder));
    } else if (auto const *inst =
                   instruction.if_as<ir::StoreInstruction<uint32_t>>()) {
      to_llvm.builder.CreateStore(
          to_llvm.Lookup(inst->value, to_llvm.builder),
          to_llvm.Lookup(inst->location, to_llvm.builder));
    } else if (auto const *inst =
                   instruction.if_as<ir::StoreInstruction<int64_t>>()) {
      to_llvm.builder.CreateStore(
          to_llvm.Lookup(inst->value, to_llvm.builder),
          to_llvm.Lookup(inst->location, to_llvm.builder));
    } else if (auto const *inst =
                   instruction.if_as<ir::StoreInstruction<uint64_t>>()) {
      to_llvm.builder.CreateStore(
          to_llvm.Lookup(inst->value, to_llvm.builder),
          to_llvm.Lookup(inst->location, to_llvm.builder));
    } else {
      NOT_YET(instruction.rtti(), instruction.to_string());
    }

    instructions.remove_prefix(1);
  }
}

void FinishLlvmBasicBlock(ir::BasicBlock const *block,
                          IrToLlvmMapping &to_llvm, bool returns_void) {
  to_llvm.builder.SetInsertPoint(to_llvm.blocks.at(block));

  switch (block->jump().kind()) {
    case ir::JumpCmd::Kind::Return:
      if (returns_void) { to_llvm.builder.CreateRetVoid(); }
      return;
    case ir::JumpCmd::Kind::Uncond:
      to_llvm.builder.CreateBr(to_llvm.blocks.at(block->jump().UncondTarget()));
      return;
    case ir::JumpCmd::Kind::Cond:
      to_llvm.builder.CreateCondBr(
          to_llvm.registers.at(block->jump().CondReg()),
          to_llvm.blocks.at(block->jump().CondTarget(true)),
          to_llvm.blocks.at(block->jump().CondTarget(false)));
      return;
    case ir::JumpCmd::Kind::Choose:
    default: UNREACHABLE("choose-jump should not be possible.");
  }
}

}  // namespace

llvm::Function *DeclareLlvmFunction(ir::CompiledFn const &fn,
                                    compiler::CompiledModule const &module,
                                    llvm::Module &llvm_module) {
  return llvm::Function::Create(llvm::cast<llvm::FunctionType>(ToLlvmType(
                                    fn.type(), llvm_module.getContext())),
                                llvm::Function::PrivateLinkage, "fn",
                                &llvm_module);
}

void EmitLlvmFunction(llvm::IRBuilder<> &builder, llvm::LLVMContext &context,
                      ir::CompiledFn const &fn, 
                      absl::flat_hash_map<ir::CompiledFn const *,
                                          llvm::Function *> const &fn_map) {
  llvm::Function &llvm_fn = *ASSERT_NOT_NULL(fn_map.at(&fn));
  IrToLlvmMapping mapping{.builder = builder, .fn_map = fn_map};

  auto iter = fn.blocks().begin();
  auto *entry_block =
      llvm::BasicBlock::Create(builder.getContext(), "entry", &llvm_fn);
  mapping.blocks.emplace(*iter, entry_block);
  ++iter;
  for (; iter != fn.blocks().end(); ++iter) {
    mapping.blocks.emplace(*iter, llvm::BasicBlock::Create(builder.getContext(),
                                                           "block", &llvm_fn));
  }

  builder.SetInsertPoint(entry_block);
  fn.for_each_alloc([&](type::Type t, ir::Reg r) {
    mapping.registers.emplace(
        r, mapping.builder.CreateAlloca(ToLlvmType(t, context)));
  });

  std::vector<std::pair<ir::BasicBlock const *, absl::Span<ir::Inst const>>>
      to_process;
  for (auto const *block : fn.blocks()) {
    to_process.emplace_back(block, block->instructions());
  }

  auto front_iter = to_process.begin();
  auto back_iter  = to_process.end();
  while (front_iter != back_iter) {
    while (front_iter != back_iter) {
      auto &[block, instructions] = *front_iter;
      TryEmitLlvmBasicBlock(block, instructions, mapping);
      if (instructions.empty()) {
        *front_iter = *back_iter;
        --back_iter;
      } else {
        ++back_iter;
      }
    }
    front_iter = to_process.begin();
  }

  bool returns_void = fn.type()->output().empty();
  for (auto const *block : fn.blocks()) {
    FinishLlvmBasicBlock(block, mapping, returns_void);
  }
}

}  // namespace backend
