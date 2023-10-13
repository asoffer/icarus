#ifndef ICARUS_IR_EMIT_H
#define ICARUS_IR_EMIT_H

#include <span>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "ir/dependent_modules.h"
#include "ir/module.h"
#include "ir/module_id.h"
#include "jasmin/value_stack.h"
#include "nth/base/attributes.h"
#include "nth/container/interval_map.h"
#include "parser/parse_tree.h"
#include "type/type.h"

namespace ic {

struct EmitContext {
  explicit EmitContext(ParseTree const& tree NTH_ATTRIBUTE(lifetimebound),
                       DependentModules const& modules
                           NTH_ATTRIBUTE(lifetimebound),
                       IrFunction& f)
      : tree(tree), function_stack{&f}, modules(modules) {}
  explicit EmitContext(ParseTree const& tree NTH_ATTRIBUTE(lifetimebound),
                       DependentModules const& modules
                           NTH_ATTRIBUTE(lifetimebound),
                       Module& module)
      : tree(tree), function_stack{&module.initializer()}, modules(modules) {}

  Module const& module(ModuleId id) const { return modules[id]; }

  void Push(jasmin::Value v, type::Type);

  void Evaluate(nth::interval<ParseTree::Node::Index> subtree,
                jasmin::ValueStack& value_stack);

  ParseTree::Node const& Node(ParseTree::Node::Index index) {
    return tree[index];
  }

  ParseTree const& tree;

  absl::flat_hash_map<ParseTree::Node, type::QualifiedType>
      statement_qualified_type;
  std::vector<Token::Kind> operator_stack;
  std::vector<IrFunction*> function_stack;
  absl::flat_hash_map<ParseTree::Node::Index, size_t> rotation_count;

  struct ComputedConstant {
    explicit ComputedConstant(ParseTree::Node::Index index,
                              jasmin::ValueStack value)
        : index_(index), value_(std::move(value)) {}

    friend bool operator==(ComputedConstant const& lhs,
                           ComputedConstant const& rhs) {
      return lhs.index_ == rhs.index_;
    }

    friend bool operator!=(ComputedConstant const& lhs,
                           ComputedConstant const& rhs) {
      return not(lhs == rhs);
    }

    std::span<jasmin::Value const> value_span() const {
      return std::span<jasmin::Value const>(value_.begin(), value_.end());
    }

   private:
    ParseTree::Node::Index index_;
    jasmin::ValueStack value_;
  };

  // Maps node indices to the constant value associated with the computation for
  // the largest subtree containing it whose constant value has been computed
  // thus far.
  nth::interval_map<ParseTree::Node::Index, ComputedConstant> constants;
  DependentModules const& modules;
};

void EmitIr(nth::interval<ParseTree::Node::Index> node_range,
            EmitContext& context);

}  // namespace ic

#endif  // ICARUS_IR_EMIT_H
