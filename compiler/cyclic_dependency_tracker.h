#ifndef ICARUS_COMPILER_CYCLIC_DEPENDENCY_TRACKER_H
#define ICARUS_COMPILER_CYCLIC_DEPENDENCY_TRACKER_H

#include <vector>
#include <iterator>

#include "compiler/data.h"
#include "absl/types/span.h"
#include "ast/ast.h"
#include "diagnostic/consumer/consumer.h"

namespace compiler {
namespace {

struct CyclicDependency {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "cyclic-dependency";

  diagnostic::DiagnosticMessage ToMessage(frontend::Source const *src) const {
    diagnostic::SourceQuote quote(src);
    for (auto const *id : cycle) {
      quote = quote.Highlighted(id->range(), diagnostic::Style::ErrorText());
    }

    return diagnostic::DiagnosticMessage(
        diagnostic::Text("Found a cyclic dependency:"), std::move(quote));
  }

  std::vector<ast::Identifier const *> cycle;
};

}  // namespace

// During type-verification, when a dependency on an identifier is encountered,
// we write it down here. If the same dependency is encountered more than once,
// it means we have a cycle in the dependency graph and should emit a
// diagnostic.
struct CyclicDependencyTracker {
 public:
  // Returned by `PushDependency`. Removes the dependency from the tracker when
  // it is destroyed.
  struct DependencyToken {
    DependencyToken(DependencyToken const &) = delete;
    DependencyToken(DependencyToken &&d)
        : ptr_(std::exchange(d.ptr_, nullptr)) {}

    explicit operator bool() { return ptr_; }
    ~DependencyToken() {
      if (ptr_) {
        ASSERT(ptr_->dependencies_.size() != 0u);
        ptr_->dependencies_.pop_back();
      }
    }

   private:
    friend CyclicDependencyTracker;
    DependencyToken(CyclicDependencyTracker *ptr = nullptr) : ptr_(ptr) {}

    CyclicDependencyTracker *ptr_ = nullptr;
  };

  DependencyToken PushDependency(ast::Identifier const *id,
                                 DependentComputedData &data,
                                 diagnostic::DiagnosticConsumer &diag) {
    dependencies_.push_back(id);
    auto iter = dependencies_.begin();
    for (; iter != dependencies_.end(); ++iter) {
      if (*iter == id) { break; }
    }

    if (iter == std::prev(dependencies_.end())) {
      return DependencyToken(this);
    }

    diag.Consume(CyclicDependency{
        .cycle = {iter, dependencies_.end()},
    });
    for (; iter != dependencies_.end(); ++iter) {
      data.set_cyclic_error(*iter);
    }

    return DependencyToken();
  }

 private:
  friend DependencyToken;
  std::vector<ast::Identifier const *> dependencies_;
};

}  // namespace compiler

#endif  // ICARUS_COMPILER_CYCLIC_DEPENDENCY_TRACKER_H