#ifndef ICARUS_IR_CMD_INLINER_H
#define ICARUS_IR_CMD_INLINER_H

#include "absl/container/flat_hash_map.h"
#include "ir/block_group.h"
#include "ir/reg.h"
#include "ir/results.h"

namespace type {
struct Type;
}  // namespace type

namespace ir {
struct BasicBlock;
struct BlockDef;
struct CompiledFn;
struct StackFrameAllocations;

struct Inliner {
  static Inliner Make(internal::BlockGroup *group) {
    BasicBlock *block = group->blocks().back();
    group->AppendBlock();
    return Inliner(group->reg_to_offset_.size(), group->blocks().size() - 1,
                   block);
  }

  void Inline(Reg *r, type::Type const *t = nullptr) const;

  constexpr void Inline(BasicBlock const *b) const {
    // TODO *b = BlockIndex(b->value + block_offset_);
  }

  void MergeAllocations(internal::BlockGroup *group,
                        StackFrameAllocations const &allocs);

  BasicBlock *landing() const { return land_; }

 private:
  friend struct ::ir::internal::BlockGroup;
  explicit Inliner(size_t reg_offset, size_t block_offset, BasicBlock *land)
      : reg_offset_(reg_offset), block_offset_(block_offset), land_(land) {}

  size_t reg_offset_   = 0;
  size_t block_offset_ = 0;
  BasicBlock *land_    = nullptr;
};

std::pair<Results, bool> CallInline(
    CompiledFn *f, Results const &arguments,
    absl::flat_hash_map<BlockDef const *, BasicBlock *> const &block_map);

}  // namespace ir

#endif  // ICARUS_IR_CMD_INLINER_H