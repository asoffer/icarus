#include "absl/cleanup/cleanup.h"
#include "ast/ast.h"
#include "compiler/compiler.h"
#include "compiler/emit/copy_move_assignment.h"
#include "diagnostic/message.h"
#include "type/generic.h"

namespace compiler {
namespace {

struct PatternMatchFailure {
  static constexpr std::string_view kCategory = "pattern-error";
  static constexpr std::string_view kName     = "pattern-match-fail";

  diagnostic::DiagnosticMessage ToMessage() const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text("Failed to match type of function parameter against "
                         "provided argument of type `%s`.",
                         type),
        diagnostic::SourceQuote().Highlighted(range,
                                              diagnostic::Style::ErrorText()));
  }

  std::string type;
  std::string_view range;
};

}  // namespace

void Compiler::EmitToBuffer(ast::PatternMatch const *node,
                            ir::PartialResultBuffer &out) {
  if (not node->is_binary()) {
    EmitToBuffer(&node->pattern(), out);
    ir::RegOr<ir::Interface> intf = out.back().get<ir::Interface>();
    out.pop_back();
    out.append(current_block()->Append(type::GenericTypeInstruction{
        .interface = intf, .result = current().subroutine->Reserve()}));
    return;
  }

  type::Type t                    = context().qual_types(node)[0].type();
  ir::CompleteResultBuffer buffer = *EvaluateToBufferOrDiagnose(
      type::Typed<ast::Expression const *>(&node->expr(), t));
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
    for (auto const &id :
         node->scope()->visible_ancestor_declaration_id_named(name->name())) {
      LOG("PatternMatch", "Binding %s (%p) to %s on %s", name->name(), &id,
          context().qual_types(&id)[0].type().Representation(value[0]),
          context().DebugString());
      context().SetConstant(&id, std::move(value));
      return;
    }
  }
}

void Compiler::EmitCopyAssign(
    ast::PatternMatch const *node,
    absl::Span<type::Typed<ir::RegOr<ir::addr_t>> const> to) {
  auto t = context().qual_types(node)[0].type();
  ASSERT(to.size() == 1u);
  ir::PartialResultBuffer buffer;
  EmitToBuffer(node, buffer);
  CopyAssignmentEmitter emitter(*this);
  emitter(to[0], type::Typed(buffer[0], t));
}

void Compiler::EmitMoveAssign(
    ast::PatternMatch const *node,
    absl::Span<type::Typed<ir::RegOr<ir::addr_t>> const> to) {
  auto t = context().qual_types(node)[0].type();
  ASSERT(to.size() == 1u);
  ir::PartialResultBuffer buffer;
  EmitToBuffer(node, buffer);
  MoveAssignmentEmitter emitter(*this);
  emitter(to[0], type::Typed(buffer[0], t));
}

void Compiler::EmitCopyInit(
    ast::PatternMatch const *node,
    absl::Span<type::Typed<ir::RegOr<ir::addr_t>> const> to) {
  auto t = context().qual_types(node)[0].type();
  ASSERT(to.size() == 1u);
  ir::PartialResultBuffer buffer;
  EmitToBuffer(node, buffer);
  CopyAssignmentEmitter emitter(*this);
  emitter(to[0], type::Typed(buffer[0], t));
}

void Compiler::EmitMoveInit(
    ast::PatternMatch const *node,
    absl::Span<type::Typed<ir::RegOr<ir::addr_t>> const> to) {
  auto t = context().qual_types(node)[0].type();
  ASSERT(to.size() == 1u);
  ir::PartialResultBuffer buffer;
  EmitToBuffer(node, buffer);
  MoveAssignmentEmitter emitter(*this);
  emitter(to[0], type::Typed(buffer[0], t));
}

}  // namespace compiler
