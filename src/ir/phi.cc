#include "ir/phi.h"

#include "type/pointer.h"
#include "type/struct.h"

namespace IR {
template <typename T>
static std::unordered_map<BlockIndex, RegisterOr<T>> ConvertMap(
    const std::unordered_map<BlockIndex, Val> &val_map) {
  std::unordered_map<BlockIndex, RegisterOr<T>> result;

  for (const auto & [ block, val ] : val_map) {
    result.emplace(block, val.template reg_or<T>());
  }

  return result;
}

template <typename T>
static std::unique_ptr<PhiArgs<T>> MakePhiArgs(
    const std::unordered_map<BlockIndex, IR::Val> &val_map) {
  auto phi_args = std::make_unique<PhiArgs<T>>();
  for (const auto & [ block, val ] : val_map) {
    phi_args->map_.emplace(block, val.template reg_or<T>());
  }
  return phi_args;
}

CmdIndex Phi(type::Type const *t) {
  CmdIndex cmd_index{
      BasicBlock::Current,
      static_cast<i32>(Func::Current->block(BasicBlock::Current).cmds_.size())};
  ASSERT_NOT_NULL(Func::Current)
      ->block(BasicBlock::Current)
      .cmds_.emplace_back(t, Op::Death);
  return cmd_index;
}

Val MakePhi(CmdIndex phi_index,
            const std::unordered_map<BlockIndex, IR::Val> &val_map) {
  auto &cmd = IR::Func::Current->Command(phi_index);
  auto *cmd_type  = val_map.begin()->second.type;

  if (cmd_type == type::Bool) {
    return IR::ValFrom(MakePhi<bool>(phi_index, ConvertMap<bool>(val_map)));
  } else if (cmd_type == type::Char) {
    return IR::ValFrom(MakePhi<char>(phi_index, ConvertMap<char>(val_map)));
  } else if (cmd_type == type::Int) {
    return IR::ValFrom(MakePhi<int>(phi_index, ConvertMap<int>(val_map)));
  } else if (cmd_type == type::Real) {
    return IR::ValFrom(MakePhi<double>(phi_index, ConvertMap<double>(val_map)));
  } else if (cmd_type == type::Type_) {
    return IR::ValFrom(MakePhi<type::Type const *>(
        phi_index, ConvertMap<type::Type const *>(val_map)));
  } else if (cmd_type->is<type::Pointer>()) {
    return IR::ValFrom(
        MakePhi<IR::Addr>(phi_index, ConvertMap<IR::Addr>(val_map)),
        &cmd_type->as<type::Pointer>());
  } else if (cmd_type == type::Block || cmd_type == type::OptBlock) {
    auto phi_args  = MakePhiArgs<BlockSequence>(val_map);
    cmd.op_code_   = Op::PhiBlock;
    cmd.phi_block_ = Cmd::PhiBlock::Make(phi_args.get());
    IR::Func::Current->block(BasicBlock::Current)
        .phi_args_.push_back(std::move(phi_args));
  } else {
    NOT_YET(cmd_type->to_string());
  }
  return IR::Val::Reg(cmd.result, val_map.begin()->second.type);
}

}  // namespace IR
