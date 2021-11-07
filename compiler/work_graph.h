#ifndef ICARUS_COMPILER_WORK_GRAPH_H
#define ICARUS_COMPILER_WORK_GRAPH_H

#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "ast/expression.h"
#include "ast/node.h"
#include "compiler/compiler.h"
#include "compiler/emit/common.h"
#include "compiler/module.h"
#include "compiler/resources.h"
#include "compiler/work_item.h"
#include "diagnostic/consumer/buffering.h"
#include "type/typed_value.h"

namespace compiler {

// A `WorkGraph` is a data structure that tracks dependencies between work items
// as well as which work items have already been completed.
struct WorkGraph {
  // `WorkGraph` will construct Compilers that have references back to itself
  // through the `enqueue` callable installed on `PersistentResources`. This
  // makes it unsafe to either copy or move a `workGraph`.
  WorkGraph(WorkGraph const &) = delete;
  WorkGraph(WorkGraph &&)      = delete;
  WorkGraph &operator=(WorkGraph const &) = delete;
  WorkGraph &operator=(WorkGraph &&) = delete;

  explicit WorkGraph(PersistentResources const &resources)
      : resources_(resources) {}

  bool ExecuteCompilationSequence(
      Context &context, base::PtrSpan<ast::Node const> nodes,
      std::invocable<WorkGraph &, base::PtrSpan<ast::Node const>> auto
          &&... steps) {
    bool should_continue = true;
    (void)((should_continue = steps(*this, nodes), complete(),
            should_continue) and
           ...);
    return should_continue;
  }

  void emplace(WorkItem const &w,
               absl::flat_hash_set<WorkItem> const &dependencies = {}) {
    dependencies_.emplace(w, dependencies);
  }

  void emplace(WorkItem const &w,
               absl::flat_hash_set<WorkItem> &&dependencies) {
    dependencies_.emplace(w, std::move(dependencies));
  }

  // Ensure that the given `WorkItem` has been completed. If the item had
  // previously been executed, nothing happens. If the item has not been
  // previously executed, this function will also ensure that all transitively
  // depended-on `WorkItem`s are executed before executing `w`.
  bool Execute(WorkItem const &w);

  // Complete all work in the work queue.
  void complete() {
    while (not dependencies_.empty()) {
      // It's important to copy the item because calling `execute` might cause
      // `dependencies_` to rehash.
      auto item = dependencies_.begin()->first;
      Execute(item);
    }
  }

  PersistentResources const &resources() const { return resources_; }

  std::variant<ir::CompleteResultBuffer,
               std::vector<diagnostic::ConsumedMessage>>
  EvaluateToBuffer(Context &context, type::Typed<ast::Expression const *> expr,
                   bool must_complete);

  WorkResources work_resources() {
    return {
      .enqueue = [this](WorkItem item,
                        absl::flat_hash_set<WorkItem> prerequisites) {
        emplace(item, std::move(prerequisites));
      },
      .evaluate = std::bind_front(&WorkGraph::EvaluateToBuffer, this),
      .complete = std::bind_front(&WorkGraph::Execute, this),
    };
  }

 private:
  PersistentResources resources_;
  absl::flat_hash_map<WorkItem, absl::flat_hash_set<WorkItem>> dependencies_;
  absl::flat_hash_map<WorkItem, bool> work_;
};

bool CompileLibrary(Context &context, PersistentResources const &resources,
                    base::PtrSpan<ast::Node const> nodes);
std::optional<ir::CompiledFn> CompileExecutable(
    Context &context, PersistentResources const &resources,
    base::PtrSpan<ast::Node const> nodes);

}  // namespace compiler

#endif  // ICARUS_COMPILER_WORK_GRAPH_H