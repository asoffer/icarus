#ifndef ICARUS_COMPILER_DISPATCH_RUNTIME_H
#define ICARUS_COMPILER_DISPATCH_RUNTIME_H

#include "absl/container/flat_hash_map.h"
#include "base/meta.h"
#include "core/fn_args.h"
#include "core/fn_params.h"
#include "ir/basic_block.h"
#include "ir/builder.h"
#include "ir/components.h"
#include "ir/reg_or.h"
#include "ir/results.h"
#include "type/typed_value.h"

namespace compiler {

// Emits code which determines if a function with parameters `params` should be
// called with arguments `args`. It does this by looking for variants in `args`
// and testing the actually held type to see if it matches the corresponding
// parameter type. Note that the parameter type need not be identical. Rather,
// there must be a cast from the actual argument type to the parameter type
// (usually due to a cast such as `int64` casting to `int64 | bool`).
ir::RegOr<bool> EmitRuntimeDispatchOneComparison(
    ir::Builder &bldr, core::FnParams<type::Type const *> const &params,
    core::FnArgs<type::Typed<ir::Results>> const &args);

// Emits code which jumps to the appropriate argument-prep-and-function-call
// after testing variants for the right type.
template <typename Key, typename Value>
void EmitRuntimeDispatch(
    ir::Builder &bldr, absl::flat_hash_map<Key, Value> const &table,
    absl::flat_hash_map<Key, ir::BasicBlock *> const &callee_to_block,
    core::FnArgs<type::Typed<ir::Results>> const &args) {
  // TODO This is a simple linear search through the table which is certainly a
  // bad idea. We can optimize it later. Likely the right way to do this is to
  // find a perfect hash of the function variants that produces an index into a
  // block table so we pay for a hash and a single indirect jump. This may be
  // harder if you remove variant and implement `overlay`.

  auto iter = table.begin();

  while (true) {
    auto const &[key, val] = *iter;
    ++iter;

    auto &block = callee_to_block.at(key);
    if (iter == table.end()) {
      bldr.UncondJump(block);
      break;
    }

    core::FnParams<type::Type const *> params;
    if constexpr (std::is_same_v<Key, ast::Expression const *>) {
      params = val.params().Transform([](auto const &p) { return p.type(); });
    } else if constexpr (std::is_same_v<Key, ir::Jump *>) {
      params = key->params().Transform([](auto const &p) { return p.type(); });
    } else {
      static_assert(base::always_false<Key>());
    }

    ir::RegOr<bool> match =
        EmitRuntimeDispatchOneComparison(bldr, params, args);
    bldr.CurrentBlock() = ir::EarlyExitOn<true>(block, match);
  }
}

}  // namespace compiler

#endif  // ICARUS_COMPILER_DISPATCH_RUNTIME_H