#ifndef ICARUS_IR_INSTRUCTION_CORE_H
#define ICARUS_IR_INSTRUCTION_CORE_H

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "base/extend.h"
#include "base/extend/serialize.h"
#include "base/serialize.h"
#include "ir/blocks/basic.h"
#include "ir/instruction/debug.h"
#include "ir/instruction/inliner.h"
#include "ir/instruction/op_codes.h"
#include "ir/interpreter/architecture.h"
#include "ir/interpreter/byte_code_reader.h"
#include "ir/interpreter/byte_code_writer.h"
#include "ir/out_params.h"
#include "ir/value/reg_or.h"

namespace ir {
// These instructions are required to appear in every instruction set. They
// consist of any instruction having to do with control-flow or reading/writing
// memory.

struct LoadInstruction
    : base::Extend<LoadInstruction>::With<
          InlineExtension, base::BaseSerializeExtension, DebugFormatExtension> {
  static constexpr cmd_index_t kIndex = internal::kLoadInstructionNumber;
  static constexpr std::string_view kDebugFormat = "%3$s = load %2$s (%1$s)";

  type::Type type;
  RegOr<addr_t> addr;
  Reg result;
};

// TODO consider changing these to something like 'basic block arguments'
template <typename T>
struct PhiInstruction {
  using type = T;

  PhiInstruction() = default;
  PhiInstruction(std::vector<BasicBlock const*> blocks,
                 std::vector<RegOr<T>> values)
      : blocks(std::move(blocks)), values(std::move(values)) {}

  void add(BasicBlock const* block, RegOr<T> value) {
    blocks.push_back(block);
    values.push_back(value);
  }

  friend void BaseSerialize(interpreter::ByteCodeWriter& w,
                            PhiInstruction const& inst) {
    base::Serialize(w, static_cast<uint16_t>(inst.values.size()));
    for (auto block : inst.blocks) { base::Serialize(w, block); }
    for (auto value : inst.values) { base::Serialize(w, value); }
    base::Serialize(w, inst.result);
  }

  std::string to_string() const {
    using base::stringify;
    std::string s = absl::StrCat(stringify(result), " = phi ");
    for (size_t i = 0; i < blocks.size(); ++i) {
      absl::StrAppend(&s, "\n      ", stringify(blocks[i]), ": ",
                      stringify(values[i]));
    }
    return s;
  }

  void Inline(InstructionInliner const& inliner) {
    inliner.Inline(values);
    inliner.Inline(result);
  }

  std::vector<BasicBlock const*> blocks;
  std::vector<RegOr<T>> values;
  Reg result;
};

// This instruction is a bit strange sets a register to either another registor,
// or an immediate value. By the very nature of Single-Static-Assignment, every
// use of this instruction is an optimization opportunity. If a register is
// initialized with an immediate value, we can do constant propagation. If it is
// initialized with another register, the two registers can be folded into a
// single register.
//
// The benefit of such an instruction is that it enables us to inline code
// without worrying about rewriting register names immediately. This instruction
// should never be visible in the final code.
template <typename T>
struct RegisterInstruction
    : base::Extend<RegisterInstruction<T>>::template With<
          base::BaseSerializeExtension, InlineExtension, DebugFormatExtension> {
  static constexpr std::string_view kDebugFormat = "%2$s = %1$s";

  T Resolve() const { return Apply(operand.value()); }
  static T Apply(T val) { return val; }

  RegOr<T> operand;
  Reg result;
};

template <typename T>
struct SetReturnInstruction
    : base::Extend<SetReturnInstruction<T>>::template With<
          base::BaseSerializeExtension, InlineExtension, DebugFormatExtension> {
  using type                                     = T;
  static constexpr std::string_view kDebugFormat = "set-ret %1$s = %2$s";

  uint16_t index;
  RegOr<T> value;
};

template <typename T>
struct StoreInstruction
    : base::Extend<StoreInstruction<T>>::template With<
          base::BaseSerializeExtension, InlineExtension, DebugFormatExtension> {
  static constexpr std::string_view kDebugFormat = "store %1$s into [%2$s]";
  using type                                     = T;

  template <typename ExecContext>
  void Apply(ExecContext& ctx) {
    ctx.Store(ctx.resolve(location), ctx.resolve(value));
  }

  RegOr<T> value;
  RegOr<addr_t> location;
};

struct CallInstruction {
  CallInstruction(type::Function const* fn_type, RegOr<Fn> const& fn,
                  std::vector<Value> args, OutParams outs)
      : fn_type_(fn_type),
        fn_(fn),
        args_(std::move(args)),
        outs_(std::move(outs)) {
    ASSERT(this->outs_.size() == fn_type_->output().size());
    ASSERT(args_.size() == fn_type_->params().size());
  }

  std::string to_string() const {
    using base::stringify;
    return absl::StrFormat(
        "%scall %s: %s",
        fn_type_->output().empty()
            ? ""
            : absl::StrCat("(",
                           absl::StrJoin(outs_.regs(), ", ",
                                         [](std::string* s, auto const& out) {
                                           absl::StrAppend(s, stringify(out));
                                         }),
                           ") = "),
        stringify(fn_),
        absl::StrJoin(args_, ", ", [](std::string* s, auto const& arg) {
          absl::StrAppend(s, stringify(arg));
        }));
  }

  friend void BaseSerialize(interpreter::ByteCodeWriter& w,
                            CallInstruction const& inst) {
    base::Serialize(w, inst.fn_, inst.args_, inst.outs_);
  }

  type::Function const* func_type() const { return fn_type_; }
  RegOr<Fn> func() const { return fn_; }
  absl::Span<Value const> arguments() const { return args_; }
  OutParams const& outputs() const { return outs_; }

  void Inline(InstructionInliner const& inliner) {
    inliner.Inline(fn_);
    for (auto& arg : args_) { inliner.Inline(arg); }
    for (auto& reg : outs_.regs()) { inliner.Inline(reg); }
  }

 private:
  type::Function const* fn_type_;
  RegOr<Fn> fn_;
  std::vector<Value> args_;
  OutParams outs_;
};

struct CommentInstruction
    : base::Extend<CommentInstruction>::With<InlineExtension,
                                             DebugFormatExtension> {
  static constexpr std::string_view kDebugFormat = "comment: %1$s";

  friend void BaseSerialize(interpreter::ByteCodeWriter& w,
                            CommentInstruction const&) {}
  friend void BaseDeserialize(interpreter::ByteCodeReader&,
                              CommentInstruction const&) {}

  template <typename ExecContext>
  void Apply(ExecContext&) const {}

  std::string comment;
};

}  // namespace ir

#endif  // ICARUS_IR_INSTRUCTION_CORE_H
