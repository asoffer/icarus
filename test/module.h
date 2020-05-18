#ifndef ICARUS_TEST_MODULE_H
#define ICARUS_TEST_MODULE_H

#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "ast/ast.h"
#include "ast/expression.h"
#include "ast/overload_set.h"
#include "base/ptr_span.h"
#include "diagnostic/consumer/trivial.h"
#include "frontend/source/range.h"
#include "module/module.h"
#include "test/util.h"

namespace test {
struct TrackingConsumer : diagnostic::DiagnosticConsumer {
  explicit TrackingConsumer() : diagnostic::DiagnosticConsumer(nullptr) {}
  ~TrackingConsumer() override {}

  void ConsumeImpl(std::string_view category, std::string_view name,
                   diagnostic::DiagnosticMessage&&) override {
    diagnostics_.emplace_back(category, name);
  }

  absl::Span<std::pair<std::string_view, std::string_view> const> diagnostics()
      const {
    return diagnostics_;
  }

 private:
  std::vector<std::pair<std::string_view, std::string_view>> diagnostics_;
};


struct TestModule : compiler::CompiledModule {
  TestModule()
      : compiler({
            .builder             = ir::GetBuilder(),
            .data                = data(),
            .diagnostic_consumer = consumer,
        }) {}
  ~TestModule() { compiler.CompleteDeferredBodies(); }

  template <typename NodeType>
  NodeType const* Append(std::string code) {
    auto node       = test::ParseAs<NodeType>(std::move(code));
    auto const* ptr = ASSERT_NOT_NULL(node.get());
    AppendNode(std::move(node), consumer);
    return ptr;
  }

  TrackingConsumer consumer;
  compiler::Compiler compiler;

 protected:
  void ProcessNodes(base::PtrSpan<ast::Node const> nodes,
                    diagnostic::DiagnosticConsumer& diag) override {
    for (ast::Node const* node : nodes) { compiler.VerifyType(node); }
    compiler.CompleteDeferredBodies();
  }
};

ast::Call const* MakeCall(
    std::string fn, std::vector<std::string> pos_args,
    absl::flat_hash_map<std::string, std::string> named_args,
    TestModule* module) {
  auto call_expr = std::make_unique<ast::Call>(
      frontend::SourceRange{}, ParseAs<ast::Expression>(std::move(fn)),
      MakeFnArgs(std::move(pos_args), std::move(named_args)));
  auto* expr = call_expr.get();
  module->AppendNode(std::move(call_expr), module->consumer);
  module->compiler.CompleteDeferredBodies();
  return expr;
}

}  // namespace test

#endif  // ICARUS_TEST_MODULE_H
