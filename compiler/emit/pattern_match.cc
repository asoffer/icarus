#include "absl/cleanup/cleanup.h"
#include "ast/ast.h"
#include "compiler/compiler.h"
#include "diagnostic/message.h"

namespace compiler {
namespace {

struct PatternMatchFailure {
  static constexpr std::string_view kCategory = "pattern-error";
  static constexpr std::string_view kName     = "pattern-match-fail";

  diagnostic::DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text("Failed to match type of function parameter against "
                         "provided argument of type `%s`.",
                         type),
        diagnostic::SourceQuote(src).Highlighted(
            range, diagnostic::Style::ErrorText()));
  }

  std::string type;
  std::string_view range;
};

}  // namespace

void Compiler::EmitToBuffer(ast::PatternMatch const *node,
                            ir::PartialResultBuffer &out) {
  ir::CompleteResultBuffer buffer;
  type::Type t;
  if (node->is_binary()) {
    t      = context().qual_types(node)[0].type();
    buffer = *EvaluateToBufferOrDiagnose(
        type::Typed<ast::Expression const *>(&node->expr(), t));
  } else {
    t = type::Type_;
    buffer.append(context().arg_type(
        node->expr().as<ast::Declaration>().ids()[0].name()));
  }
  out = buffer;

  auto &q         = state().pattern_match_queues.emplace_back();
  absl::Cleanup c = [&] { state().pattern_match_queues.pop_back(); };

  q.emplace(&node->pattern(),
            PatternMatchingContext{.type = t, .value = buffer});

  absl::flat_hash_map<ast::Declaration::Id const *, ir::CompleteResultBuffer>
      bindings;
  while (not q.empty()) {
    auto [n, pmc] = std::move(q.front());
    q.pop();

    if (not PatternMatch(n, pmc, bindings)) {
      if (node->is_binary()) {
        NOT_YET(node->DebugString());
      } else {
        diag().Consume(PatternMatchFailure{
            .type = buffer.get<type::Type>(0)
                        .to_string(),  // TODO: Use TypeForDiagnostic
            .range = node->pattern().range(),
        });
        return;
      }
    }
  }

  for (auto &[name, value] : bindings) {
    for (auto const &s : node->scope()->ancestors()) {
      auto iter = s.decls_.find(name->name());
      if (iter != s.decls_.end()) {
        auto const *id = iter->second[0];
        LOG("PatternMatch", "Binding %s (%p) to %s on %s", name->name(), id,
            context().qual_types(id)[0].type().Representation(value[0]),
            context().DebugString());
        context().SetConstant(id, std::move(value));
        return;
      }
    }
  }
}

}  // namespace compiler
