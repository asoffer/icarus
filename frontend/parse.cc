#include <array>
#include <cstdio>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ast/ast.h"
#include "base/debug.h"
#include "base/guarded.h"
#include "diagnostic/errors.h"
#include "frontend/lex/lex.h"
#include "frontend/lex/operators.h"
#include "frontend/lex/tagged_node.h"
#include "frontend/lex/token.h"
#include "frontend/parse_rule.h"
#include "frontend/source/source.h"

namespace debug {
bool parser = false;
}  // namespace debug

namespace frontend {
namespace {
using ::matcher::InheritsFrom;

std::unique_ptr<ast::Identifier> MakeInvalidNode(SourceRange range = SourceRange()) {
  return std::make_unique<ast::Identifier>(range, "invalid_node");
}

struct Statements : public ast::Node {
  Statements() {}
  ~Statements() override {}
  Statements(Statements &&) noexcept = default;
  Statements &operator=(Statements &&) noexcept = default;

  void Accept(ast::MutableVisitorBase *visitor, void *ret,
              void *arg_tuple) override {
    visitor->ErasedVisit(this, ret, arg_tuple);
  }
  void Accept(ast::VisitorBase *visitor, void *ret,
              void *arg_tuple) const override {
    visitor->ErasedVisit(this, ret, arg_tuple);
  }

  size_t size() const { return content_.size(); }
  void append(std::unique_ptr<ast::Node> &&node) {
    if (auto *stmts = node->if_as<Statements>()) {
      content_.insert(content_.end(),
                      std::make_move_iterator(stmts->content_.begin()),
                      std::make_move_iterator(stmts->content_.end()));
    } else {
      content_.push_back(std::move(node));
    }
  }
  std::vector<std::unique_ptr<ast::Node>> extract() && {
    return std::move(content_);
  }
  std::vector<std::unique_ptr<ast::Node>> content_;
};

// Temporary node which never appears in the AST but is useful during parsing to
// distinguish 'when' from other binary operators.
struct SwitchWhen : public ast::Node {
  ~SwitchWhen() override {}

  void Accept(ast::MutableVisitorBase *visitor, void *ret,
              void *arg_tuple) override {
    visitor->ErasedVisit(this, ret, arg_tuple);
  }
  void Accept(ast::VisitorBase *visitor, void *ret,
              void *arg_tuple) const override {
    visitor->ErasedVisit(this, ret, arg_tuple);
  }

  std::unique_ptr<ast::Node> body;
  std::unique_ptr<ast::Expression> cond;
};

template <typename To, typename From>
std::unique_ptr<To> move_as(std::unique_ptr<From> &val) {
  ASSERT(val, InheritsFrom<To>());
  return std::unique_ptr<To>(static_cast<To *>(val.release()));
}

void ValidateStatementSyntax(ast::Node *node,
                             diagnostic::DiagnosticConsumer &diag) {
  if (auto *cl = node->if_as<ast::CommaList>()) {
    diag.Consume(diagnostic::CommaSeparatedListStatement{.range = cl->span});
    NOT_YET();
  }
}

constexpr size_t left_assoc  = 0;
constexpr size_t right_assoc = 1;
constexpr size_t non_assoc   = 2;
constexpr size_t chain_assoc = 3;
constexpr size_t assoc_mask  = 3;

constexpr size_t precedence(Operator op) {
  switch (op) {
#define OPERATOR_MACRO(name, symbol, tag, prec, assoc)                         \
  case Operator::name:                                                         \
    return (((prec) << 2) + (assoc));
#include "frontend/lex/operators.xmacro.h"
#undef OPERATOR_MACRO
  }
  __builtin_unreachable();
}

std::unique_ptr<ast::Node> AddHashtag(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer & diag) {
  auto expr = move_as<ast::Expression>(nodes.back());
  auto iter = BuiltinHashtagMap.find(nodes.front()->as<Token>().token);
  if (iter != BuiltinHashtagMap.end()) {
    expr->hashtags_.emplace_back(iter->second);
  } else {
    diag.Consume(diagnostic::Todo{});
  }
  return expr;
}

std::unique_ptr<ast::Switch> BuildSwitch(std::unique_ptr<Statements> stmts,
                                         diagnostic::DiagnosticConsumer &diag) {
  auto switch_expr  = std::make_unique<ast::Switch>();
  switch_expr->span = stmts->span;  // TODO it's really bigger than this because
                                    // it involves the keyword too.

  switch_expr->cases_.reserve(stmts->content_.size());
  for (auto &stmt : stmts->content_) {
    if (auto *switch_when = stmt->if_as<SwitchWhen>()) {
      switch_expr->cases_.emplace_back(std::move(switch_when->body),
                                       std::move(switch_when->cond));
    } else {
      diag.Consume(diagnostic::Todo{});
    }
  }

  return switch_expr;
}

std::unique_ptr<ast::Node> OneBracedStatement(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto stmts  = std::make_unique<Statements>();
  stmts->span = SourceRange(nodes[0]->span.begin(), nodes[2]->span.end());
  stmts->append(std::move(nodes[1]));
  ValidateStatementSyntax(stmts->content_.back().get(), diag);
  return stmts;
}

std::unique_ptr<ast::Node> EmptyBraces(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &) {
  auto stmts  = std::make_unique<Statements>();
  stmts->span = SourceRange(nodes[0]->span.begin(), nodes[1]->span.end());
  return stmts;
}

std::unique_ptr<ast::Node> BuildControlHandler(
    std::unique_ptr<ast::Node> node) {
  auto &tk = node->as<Token>().token;
  if (tk == "return") { return std::make_unique<ast::ReturnStmt>(node->span); }
  UNREACHABLE();
}

std::unique_ptr<ast::Node> BracedStatementsSameLineEnd(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto stmts  = move_as<Statements>(nodes[1]);
  stmts->span = SourceRange(nodes[0]->span.begin(), nodes[2]->span.end());
  if (nodes[2]->is<Statements>()) {
    for (auto &stmt : nodes[2]->as<Statements>().content_) {
      stmts->append(std::move(stmt));
      ValidateStatementSyntax(stmts->content_.back().get(), diag);
    }
  } else {
    stmts->append(std::move(nodes[2]));
    ValidateStatementSyntax(stmts->content_.back().get(), diag);
  }
  return stmts;
}

std::unique_ptr<ast::Node> BracedStatementsJumpSameLineEnd(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  nodes[2] = BuildControlHandler(std::move(nodes[2]));
  return BracedStatementsSameLineEnd(std::move(nodes), diag);
}

std::unique_ptr<ast::Node> BuildRightUnop(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  const std::string &tk = nodes[1]->as<Token>().token;
  if (tk == ":?") {
    SourceRange span(nodes[0]->span.begin(), nodes[1]->span.end());
    auto unop = std::make_unique<ast::Unop>(span, Operator::TypeOf,
                                            move_as<ast::Expression>(nodes[0]));

    if (unop->operand()->is<ast::Declaration>()) {
      diag.Consume(diagnostic::DeclarationUsedInUnaryOperator{
          .range = unop->operand()->span,
      });
    }

    return unop;
  } else {
    UNREACHABLE();
  }
}

std::unique_ptr<ast::Node> BuildCallImpl(
    SourceRange span, std::unique_ptr<ast::Expression> callee,
    std::unique_ptr<ast::Expression> args_expr,
    diagnostic::DiagnosticConsumer &diag) {
  if (not args_expr) {
    return std::make_unique<ast::Call>(std::move(span), std::move(callee),
                                       core::OrderedFnArgs<ast::Expression>{});
  }

  std::vector<std::pair<std::string, std::unique_ptr<ast::Expression>>> args;
  if (auto *cl = args_expr->if_as<ast::CommaList>()) {
    std::optional<SourceRange> last_named_range_before_error = std::nullopt;
    std::vector<SourceRange> positional_error_ranges;

    for (auto &expr : cl->exprs_) {
      if (auto *b = expr->if_as<ast::Binop>();
          b and b->op() == Operator::Assign) {
        if (positional_error_ranges.empty()) {
          last_named_range_before_error = b->lhs()->span;
        }
        auto [lhs, rhs] = std::move(*b).extract();
        args.emplace_back(std::string{lhs->as<ast::Identifier>().token()},
                          std::move(rhs));
      } else {
        if (last_named_range_before_error.has_value()) {
          positional_error_ranges.push_back(expr->span);
        }
        args.emplace_back("", std::move(expr));
      }
    }

    if (not positional_error_ranges.empty()) {
      diag.Consume(diagnostic::PositionalArgumentFollowingNamed{
          .pos_ranges = positional_error_ranges,
          .last_named = *last_named_range_before_error,
      });
    }
  } else {
    if (ast::Binop *b = args_expr->if_as<ast::Binop>();
        b and b->op() == Operator::Assign) {
      auto [lhs, rhs] = std::move(*b).extract();
      args.emplace_back(std::string{lhs->as<ast::Identifier>().token()},
                        std::move(rhs));
    } else {
      args.emplace_back("", std::move(args_expr));
    }
  }

  if (callee->is<ast::Declaration>()) {
    diag.Consume(diagnostic::CallingDeclaration{
        .range = callee->span,
    });
  }

  return std::make_unique<ast::Call>(
      std::move(span), std::move(callee),
      core::OrderedFnArgs<ast::Expression>(std::move(args)));
}

std::unique_ptr<ast::Node> BuildCall(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  SourceRange span(nodes.front()->span.begin(), nodes.back()->span.end());
  return BuildCallImpl(std::move(span), move_as<ast::Expression>(nodes[0]),
                       move_as<ast::Expression>(nodes[2]), diag);
}

std::unique_ptr<ast::Node> BuildLeftUnop(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  const std::string &tk = nodes[0]->as<Token>().token;

  if (tk == "import") {
    auto span = SourceRange(nodes[0]->span.begin(), nodes[1]->span.end());
    return std::make_unique<ast::Import>(std::move(span),
                                         move_as<ast::Expression>(nodes[1]));
  } else if (tk == "goto") {
    auto range =
        SourceRange(nodes.front()->span.begin(), nodes.back()->span.end());
    std::vector<std::unique_ptr<ast::Expression>> exprs;
    std::vector<std::unique_ptr<ast::Call>> call_exprs;
    if (auto *c = nodes[1]->if_as<ast::ChainOp>();
        c and not c->parenthesized_) {
      exprs = std::move(*c).extract();
      for (auto &expr : exprs) {
        if (expr->is<ast::Call>()) {
          call_exprs.push_back(move_as<ast::Call>(expr));
        } else {
          diag.Consume(diagnostic::Todo{});
        }
      }
    } else {
      if (nodes[1]->is<ast::Call>()) {
        call_exprs.push_back(move_as<ast::Call>(nodes[1]));
      } else {
        diag.Consume(diagnostic::Todo{});
      }
    }
    return std::make_unique<ast::Goto>(std::move(range), std::move(call_exprs));

  } else if (tk == "'") {
    SourceRange span(nodes.front()->span.begin(), nodes.back()->span.end());
    return BuildCallImpl(std::move(span), move_as<ast::Expression>(nodes[1]),
                         nullptr, diag);
  }

  static absl::flat_hash_map<std::string_view, Operator> const kUnopMap{
      {"*", Operator::Mul},         {"[*]", Operator::BufPtr},
      {"@", Operator::At},          {"import", Operator::Import},
      {"&", Operator::And},         {"which", Operator::Which},
      {"-", Operator::Sub},         {"needs", Operator::Needs},
      {"!", Operator::Not},         {"copy", Operator::Copy},
      {"ensure", Operator::Ensure}, {"move", Operator::Move},
      {"$", Operator::Eval},        {"..", Operator::VariadicPack},
  };

  SourceRange range(nodes[0]->span.begin(), nodes[1]->span.end());

  auto& operand = nodes[1];
  Operator op   = kUnopMap.find(tk)->second;

  if (operand->is<ast::Declaration>()) {
    diag.Consume(diagnostic::DeclarationUsedInUnaryOperator{
        .range = range,
    });
    return std::make_unique<ast::Unop>(range, op, MakeInvalidNode(nodes[1]->span));

  } else if (not operand->is<ast::Expression>()) {
    diag.Consume(diagnostic::Todo{});
    return std::make_unique<ast::Unop>(range, op, MakeInvalidNode(nodes[1]->span));

  } else {
    return std::make_unique<ast::Unop>(range, op,
                                       move_as<ast::Expression>(nodes[1]));
  }
}

std::unique_ptr<ast::Node> BuildLabeledYield(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto span =
      SourceRange(nodes.front()->span.begin(), nodes.back()->span.end());

  std::vector<std::unique_ptr<ast::Expression>> exprs;
  if (nodes.size() > 2) {
    if (auto *cl = nodes[2]->if_as<ast::CommaList>();
        cl and not cl->parenthesized_) {
      exprs = std::move(*cl).extract();
    } else {
      exprs.push_back(move_as<ast::Expression>(nodes[1]));
    }
  }

  auto stmts  = std::make_unique<Statements>();
  stmts->span = span;
  stmts->append(std::make_unique<ast::YieldStmt>(
      std::move(span), std::move(exprs), move_as<ast::Label>(nodes[0])));
  return stmts;
}

std::unique_ptr<ast::Node> BuildUnlabeledYield(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto span =
      SourceRange(nodes.front()->span.begin(), nodes.back()->span.end());
  std::vector<std::unique_ptr<ast::Expression>> exprs;
  if (auto *cl = nodes[1]->if_as<ast::CommaList>();
      cl and not cl->parenthesized_) {
    exprs = std::move(*cl).extract();
  } else {
    exprs.push_back(move_as<ast::Expression>(nodes[1]));
  }

  auto stmts  = std::make_unique<Statements>();
  stmts->span = span;
  stmts->append(std::make_unique<ast::YieldStmt>(span, std::move(exprs)));
  return stmts;
}

std::unique_ptr<ast::Node> BuildChainOp(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto op = nodes[1]->as<Token>().op;
  std::unique_ptr<ast::ChainOp> chain;

  // Add to a chain so long as the precedence levels match. The only thing at
  // that precedence level should be the operators which can be chained.
  if (nodes[0]->is<ast::ChainOp>() and
      precedence(nodes[0]->as<ast::ChainOp>().ops().front()) ==
          precedence(op)) {
    chain = move_as<ast::ChainOp>(nodes[0]);

  } else {
    SourceRange span(nodes[0]->span.begin(), nodes[2]->span.end());
    chain = std::make_unique<ast::ChainOp>(std::move(span),
                                           move_as<ast::Expression>(nodes[0]));
  }

  chain->append(op, move_as<ast::Expression>(nodes[2]));
  return chain;
}

std::unique_ptr<ast::Node> BuildCommaList(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  std::unique_ptr<ast::CommaList> comma_list = nullptr;
  if (nodes[0]->is<ast::CommaList>() and
      not nodes[0]->as<ast::CommaList>().parenthesized_) {
    comma_list = move_as<ast::CommaList>(nodes[0]);
  } else {
    comma_list = std::make_unique<ast::CommaList>();
    comma_list->span =
        SourceRange(nodes[0]->span.begin(), nodes[2]->span.end());
    comma_list->exprs_.push_back(move_as<ast::Expression>(nodes[0]));
  }
  comma_list->exprs_.push_back(move_as<ast::Expression>(nodes[2]));
  comma_list->span.end() = comma_list->exprs_.back()->span.end();
  return comma_list;
}

std::unique_ptr<ast::Node> BuildAccess(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto span      = SourceRange(nodes[0]->span.begin(), nodes[2]->span.end());
  auto &&operand = move_as<ast::Expression>(nodes[0]);
  if (not nodes[2]->is<ast::Identifier>()) {
    diag.Consume(diagnostic::AccessRhsNotIdentifier{
        .range = nodes[2]->span,
    });
    return std::make_unique<ast::Access>(span, std::move(operand),
                                         "invalid_node");
  }

  return std::make_unique<ast::Access>(
      span, std::move(operand),
      std::string{nodes[2]->as<ast::Identifier>().token()});
}

std::unique_ptr<ast::Node> BuildIndexOperator(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto span  = SourceRange(nodes[0]->span.begin(), nodes[2]->span.end());
  auto index = std::make_unique<ast::Index>(std::move(span),
                                            move_as<ast::Expression>(nodes[0]),
                                            move_as<ast::Expression>(nodes[2]));

  if (index->lhs()->is<ast::Declaration>()) {
    diag.Consume(diagnostic::IndexingDeclaration{
        .range = nodes[0]->span,
    });
  }

  // TODO This check is correct except that we're using indexes as a temporary
  // state for building args in block nodes. Also this needs to check deeper.
  // For example, commalists could have declarations in them and we wouldn't
  // catch it. Probably we should have a frontend-only temporary node that
  // converts to an index or block node once we're sure we know which it is.
  // if (index->rhs()->is<ast::Declaration>()) {
  //   error_log->DeclarationInIndex(index->rhs()->span);
  // }

  return index;
}

std::unique_ptr<ast::Node> BuildEmptyArray(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  return std::make_unique<ast::ArrayLiteral>(
      SourceRange(nodes.front()->span.begin(), nodes.back()->span.end()),
      std::vector<std::unique_ptr<ast::Expression>>{});
}

std::unique_ptr<ast::Node> BuildEmptyCommaList(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto comma_list  = std::make_unique<ast::CommaList>();
  comma_list->span = SourceRange(nodes[0]->span.begin(), nodes[1]->span.end());
  return comma_list;
}

std::unique_ptr<ast::Node> BuildArrayLiteral(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  if (auto *cl = nodes[1]->if_as<ast::CommaList>();
      cl and not cl->parenthesized_) {
    return std::make_unique<ast::ArrayLiteral>(nodes[0]->span,
                                               std::move(*cl).extract());
  } else {
    return std::make_unique<ast::ArrayLiteral>(
        nodes[0]->span, move_as<ast::Expression>(nodes[1]));
  }
}

std::unique_ptr<ast::Node> BuildGenericStructType(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto result = std::make_unique<ast::StructType>(
      SourceRange(nodes.front()->span.begin(), nodes.back()->span.end()));
  if (nodes[1]->is<ast::CommaList>() and
      not nodes[1]->as<ast::CommaList>().parenthesized_) {
    result->args_ = std::move(nodes[1]->as<ast::CommaList>().exprs_);
  } else {
    result->args_.push_back(move_as<ast::Expression>(nodes[1]));
  }

  return result;
}

std::unique_ptr<ast::Node> BuildArrayType(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  if (auto *cl = nodes[1]->if_as<ast::CommaList>();
      cl and not cl->parenthesized_) {
    auto span =
        SourceRange(nodes.front()->span.begin(), nodes.back()->span.end());
    return std::make_unique<ast::ArrayType>(std::move(span),
                                            std::move(*cl).extract(),
                                            move_as<ast::Expression>(nodes[3]));
  } else {
    return std::make_unique<ast::ArrayType>(nodes[0]->span,
                                            move_as<ast::Expression>(nodes[1]),
                                            move_as<ast::Expression>(nodes[3]));
  }
}

template <bool IsConst>
std::unique_ptr<ast::Node> BuildDeclaration(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto op = nodes[1]->as<Token>().op;
  SourceRange span(nodes.front()->span.begin(), nodes.back()->span.end());
  std::string id;
  if (nodes[0]->is<ast::Identifier>()) {
    id = std::string{nodes[0]->as<ast::Identifier>().token()};
  }

  std::unique_ptr<ast::Expression> type_expr, init_val;
  if (op == Operator::Colon or op == Operator::DoubleColon) {
    type_expr = move_as<ast::Expression>(nodes[2]);
  } else {
    init_val = move_as<ast::Expression>(nodes[2]);
  }

  return std::make_unique<ast::Declaration>(
      std::move(span), std::move(id), std::move(type_expr), std::move(init_val),
      IsConst ? ast::Declaration::f_IsConst : 0);
}

std::vector<std::unique_ptr<ast::Declaration>> ExtractInputs(
    std::unique_ptr<ast::Expression> args,
    diagnostic::DiagnosticConsumer &diag) {
  std::vector<std::unique_ptr<ast::Declaration>> inputs;
  if (args->is<ast::Declaration>()) {
    inputs.push_back(move_as<ast::Declaration>(args));

  } else if (auto *decls = args->if_as<ast::CommaList>()) {
    inputs.reserve(decls->exprs_.size());

    for (auto &expr : decls->exprs_) {
      if (expr->is<ast::Declaration>()) {
        inputs.push_back(move_as<ast::Declaration>(expr));
      } else {
        diag.Consume(diagnostic::Todo{});
      }
    }
  } else {
    diag.Consume(diagnostic::Todo{});
  }
  return inputs;
}

std::unique_ptr<ast::Node> BuildFunctionLiteral(
    SourceRange span, std::vector<std::unique_ptr<ast::Declaration>> inputs,
    std::unique_ptr<ast::Expression> output, Statements &&stmts, bool is_short,
    diagnostic::DiagnosticConsumer &diag) {
  if (output == nullptr) {
    if (is_short) {
      return ast::FunctionLiteral::MakeShort(std::move(span), std::move(inputs),
                                             std::move(stmts).extract());
    } else {
      return ast::FunctionLiteral::MakeLong(std::move(span), std::move(inputs),
                                            std::move(stmts).extract());
    }
  }
  ASSERT(is_short == false);

  std::vector<std::unique_ptr<ast::Expression>> outputs;
  if (auto *cl = output->if_as<ast::CommaList>()) {
    for (auto &expr : cl->exprs_) {
      if (auto *decl = expr->if_as<ast::Declaration>()) {
        decl->flags() |=
            (ast::Declaration::f_IsFnParam | ast::Declaration::f_IsOutput);
      }
      outputs.push_back(std::move(expr));
    }
  } else {
    if (auto *decl = output->if_as<ast::Declaration>()) {
      decl->flags() |=
          (ast::Declaration::f_IsFnParam | ast::Declaration::f_IsOutput);
    }
    outputs.push_back(std::move(output));
  }
  return ast::FunctionLiteral::MakeLong(std::move(span), std::move(inputs),
                                        std::move(stmts).extract(),
                                        std::move(outputs));
}

std::unique_ptr<ast::Node> BuildDesignatedInitializer(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto span = SourceRange(nodes[0]->span.begin(), nodes.back()->span.end());
  if (auto *stmts = nodes[2]->if_as<Statements>()) {
    auto extracted_stmts = std::move(*stmts).extract();
    std::vector<std::pair<std::string, std::unique_ptr<ast::Expression>>>
        initializers;
    initializers.reserve(extracted_stmts.size());
    for (auto &stmt : extracted_stmts) {
      if (auto *binop = stmt->if_as<ast::Binop>()) {
        auto [lhs, rhs] = std::move(*binop).extract();
        if (auto *lhs_id = lhs->if_as<ast::Identifier>()) {
          initializers.emplace_back(std::move(*lhs_id).extract(),
                                    std::move(rhs));
        } else {
          diag.Consume(diagnostic::Todo{});
          continue;
        }
      } else {
        diag.Consume(diagnostic::Todo{});
        continue;
      }
    }

    return std::make_unique<ast::DesignatedInitializer>(
        std::move(span), move_as<ast::Expression>(nodes[0]),
        std::move(initializers));
  } else {
    return std::make_unique<ast::DesignatedInitializer>(
        std::move(span), move_as<ast::Expression>(nodes[0]),
        std::vector<
            std::pair<std::string, std::unique_ptr<ast::Expression>>>{});
  }
}

std::unique_ptr<ast::Node> BuildNormalFunctionLiteral(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto span   = SourceRange(nodes[0]->span.begin(), nodes.back()->span.end());
  auto *binop = &nodes[0]->as<ast::Binop>();
  auto [lhs, rhs] = std::move(*binop).extract();
  return BuildFunctionLiteral(
      std::move(span), ExtractInputs(std::move(lhs), diag), std::move(rhs),
      std::move(nodes[1]->as<Statements>()), false, diag);
}

std::unique_ptr<ast::Node> BuildInferredFunctionLiteral(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto span = SourceRange(nodes[0]->span.begin(), nodes.back()->span.end());
  return BuildFunctionLiteral(
      std::move(span), ExtractInputs(move_as<ast::Expression>(nodes[0]), diag),
      nullptr, std::move(nodes[2]->as<Statements>()), false, diag);
}

// TODO this loses syntactic information that a formatter cares about.
std::unique_ptr<ast::Node> BuildShortFunctionLiteral(
    std::unique_ptr<ast::Expression> args,
    std::unique_ptr<ast::Expression> body,
    diagnostic::DiagnosticConsumer &diag) {
  auto span   = SourceRange(args->span.begin(), body->span.end());
  auto inputs = ExtractInputs(std::move(args), diag);

  std::vector<std::unique_ptr<ast::Expression>> ret_vals;
  if (auto *cl = body->if_as<ast::CommaList>()) {
    ret_vals = std::move(*cl).extract();
  } else {
    ret_vals.push_back(std::move(body));
  }

  Statements stmts;
  stmts.append(
      std::make_unique<ast::ReturnStmt>(std::move(span), std::move(ret_vals)));
  return BuildFunctionLiteral(std::move(span), std::move(inputs), nullptr,
                              std::move(stmts), true, diag);
}

std::unique_ptr<ast::Node> BuildOneElementCommaList(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto comma_list  = std::make_unique<ast::CommaList>();
  comma_list->span = SourceRange(nodes[0]->span.begin(), nodes[3]->span.end());
  comma_list->exprs_.push_back(move_as<ast::Expression>(nodes[1]));
  comma_list->parenthesized_ = true;
  return comma_list;
}

std::unique_ptr<ast::Node> BuildStatementLeftUnop(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto stmts = std::make_unique<Statements>();
  auto range =
      SourceRange(nodes.front()->span.begin(), nodes.back()->span.end());
  stmts->span = range;

  const std::string &tk = nodes[0]->as<Token>().token;

  if (tk == "print") {
    std::vector<std::unique_ptr<ast::Expression>> exprs;
    if (auto *cl = nodes[1]->if_as<ast::CommaList>();
        cl and not cl->parenthesized_) {
      exprs = std::move(*cl).extract();
    } else {
      exprs.push_back(move_as<ast::Expression>(nodes[1]));
    }
    stmts->append(
        std::make_unique<ast::PrintStmt>(std::move(range), std::move(exprs)));
  } else if (tk == "return") {
    auto span =
        SourceRange(nodes.front()->span.begin(), nodes.back()->span.end());
    std::vector<std::unique_ptr<ast::Expression>> exprs;
    if (auto *cl = nodes[1]->if_as<ast::CommaList>();
        cl and not cl->parenthesized_) {
      exprs = std::move(*cl).extract();
    } else {
      exprs.push_back(move_as<ast::Expression>(nodes[1]));
    }
    stmts->append(
        std::make_unique<ast::ReturnStmt>(std::move(span), std::move(exprs)));
  }
  return stmts;
}

std::unique_ptr<ast::Node> BuildOneStatement(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto stmts  = std::make_unique<Statements>();
  stmts->span = nodes[0]->span;
  stmts->append(std::move(nodes[0]));
  ValidateStatementSyntax(stmts->content_.back().get(), diag);
  return stmts;
}

std::unique_ptr<ast::Node> BuildMoreStatements(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  std::unique_ptr<Statements> stmts = move_as<Statements>(nodes[0]);
  stmts->append(std::move(nodes[1]));
  ValidateStatementSyntax(stmts->content_.back().get(), diag);
  return stmts;
}

std::unique_ptr<ast::Node> OneBracedJump(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto stmts  = std::make_unique<Statements>();
  stmts->span = SourceRange(nodes[0]->span.begin(), nodes[2]->span.end());
  stmts->append(BuildControlHandler(std::move(nodes[1])));
  ValidateStatementSyntax(stmts->content_.back().get(), diag);
  return stmts;
}

std::unique_ptr<ast::Node> BuildControlHandler(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &) {
  return BuildControlHandler(std::move(nodes[0]));
}

std::unique_ptr<ast::Node> BuildScopeNode(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  SourceRange span(nodes.front()->span.begin(), nodes.back()->span.end());
  auto [callee, ordered_fn_args] =
      std::move(nodes[0]->as<ast::Call>()).extract();
  std::vector<ast::BlockNode> blocks;
  blocks.push_back(std::move(nodes[1]->as<ast::BlockNode>()));
  return std::make_unique<ast::ScopeNode>(std::move(span), std::move(callee),
                                          std::move(ordered_fn_args),
                                          std::move(blocks));
}

std::unique_ptr<ast::Node> BuildBlockNode(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto span =
      SourceRange(nodes.front()->span.begin(), nodes.back()->span.end());
  if (auto *id = nodes.front()->if_as<ast::Identifier>()) {
    return std::make_unique<ast::BlockNode>(
        std::move(span), std::string{id->token()},
        std::move(nodes.back()->as<Statements>()).extract());
  } else if (auto *index = nodes.front()->if_as<ast::Index>()) {
    auto [lhs, rhs] = std::move(*index).extract();
    std::vector<std::unique_ptr<ast::Declaration>> params;
    if (auto *cl = rhs->if_as<ast::CommaList>()) {
      auto exprs = std::move(*cl).extract();
      for (auto &expr : exprs) {
        params.push_back(move_as<ast::Declaration>(expr));
      }

    } else {
      params.push_back(move_as<ast::Declaration>(rhs));
    }
    return std::make_unique<ast::BlockNode>(
        std::move(span), std::string{lhs->as<ast::Identifier>().token()},
        std::move(params), std::move(nodes.back()->as<Statements>()).extract());

  } else {
    for (auto const &n : nodes) { DEBUG_LOG()(n->DebugString()); }
    diag.Consume(diagnostic::Todo{});
    return nullptr;
  }
}

std::unique_ptr<ast::Node> ExtendScopeNode(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  nodes[0]->as<ast::ScopeNode>().append_block_syntactically(
      std::move(nodes[1]->as<ast::BlockNode>()));
  return std::move(nodes[0]);
}

std::unique_ptr<ast::Node> SugaredExtendScopeNode(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  SourceRange span(nodes.front()->span.begin(), nodes.back()->span.end());
  auto *updated_last_scope_node = &nodes[2]->as<ast::ScopeNode>();
  std::vector<std::unique_ptr<ast::Node>> block_stmt_nodes;
  block_stmt_nodes.push_back(std::move(nodes[2]));

  nodes[0]->as<ast::ScopeNode>().append_block_syntactically(
      ast::BlockNode(std::move(span),
                     std::string{nodes[1]->as<ast::Identifier>().token()},
                     std::move(block_stmt_nodes)),
      updated_last_scope_node);
  return std::move(nodes[0]);
}

std::unique_ptr<ast::Node> BuildBinaryOperator(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  static absl::flat_hash_map<std::string_view, Operator> const kChainOps{
      {",", Operator::Comma}, {"==", Operator::Eq}, {"!=", Operator::Ne},
      {"<", Operator::Lt},    {">", Operator::Gt},  {"<=", Operator::Le},
      {">=", Operator::Ge},   {"&", Operator::And}, {"|", Operator::Or},
      {"^", Operator::Xor}};

  std::string const &tk = nodes[1]->as<Token>().token;
  if (auto iter = kChainOps.find(tk); iter != kChainOps.end()) {
    nodes[1]->as<Token>().op = iter->second;
    return (iter->second == Operator::Comma)
               ? BuildCommaList(std::move(nodes), diag)
               : BuildChainOp(std::move(nodes), diag);
  }

  if (tk == ".") {
    return BuildAccess(std::move(nodes), diag);

  } else if (tk == ":" or tk == ":=") {
    return BuildDeclaration<false>(std::move(nodes), diag);

  } else if (tk == "::" or tk == "::=") {
    return BuildDeclaration<true>(std::move(nodes), diag);

  } else if (tk == "=>") {
    return BuildShortFunctionLiteral(move_as<ast::Expression>(nodes[0]),
                                     move_as<ast::Expression>(nodes[2]), diag);
  } else if (tk == "=") {
    if (nodes[0]->is<ast::Declaration>()) {
      if (nodes[0]->as<ast::Declaration>().IsInferred()) {
        // NOTE: It might be that this was supposed to be a bool ==? How can we
        // give a good error message if that's what is intended?
        diag.Consume(diagnostic::Todo{});
        return move_as<ast::Declaration>(nodes[0]);
      }

      auto decl = move_as<ast::Declaration>(nodes[0]);
      decl->set_initial_value(move_as<ast::Expression>(nodes[2]));
      return decl;

    } else {
      return std::make_unique<ast::Binop>(move_as<ast::Expression>(nodes[0]),
                                          Operator::Assign,
                                          move_as<ast::Expression>(nodes[2]));
    }
  } else if (tk == "as") {
    SourceRange span(nodes[0]->span.begin(), nodes[2]->span.end());
    return std::make_unique<ast::Cast>(std::move(span),
                                       move_as<ast::Expression>(nodes[0]),
                                       move_as<ast::Expression>(nodes[2]));
  } else if (tk == "when") {
    auto when  = std::make_unique<SwitchWhen>();
    when->span = SourceRange(nodes[0]->span.begin(), nodes[2]->span.end());
    when->body = move_as<ast::Node>(nodes[0]);
    when->cond = move_as<ast::Expression>(nodes[2]);
    return when;
  } else if (tk == "'") {
    SourceRange span(nodes.front()->span.begin(), nodes.back()->span.end());
    return BuildCallImpl(std::move(span), move_as<ast::Expression>(nodes[2]),
                         move_as<ast::Expression>(nodes[0]), diag);
  }

  static absl::flat_hash_map<std::string_view, Operator> const kSymbols = {
      {"->", Operator::Arrow}, {"|=", Operator::OrEq},  {"&=", Operator::AndEq},
      {"^=", Operator::XorEq}, {"+=", Operator::AddEq}, {"-=", Operator::SubEq},
      {"*=", Operator::MulEq}, {"/=", Operator::DivEq}, {"%=", Operator::ModEq},
      {"+", Operator::Add},    {"-", Operator::Sub},    {"*", Operator::Mul},
      {"/", Operator::Div},    {"%", Operator::Mod}};
  return std::make_unique<ast::Binop>(move_as<ast::Expression>(nodes[0]),
                                      kSymbols.find(tk)->second,
                                      move_as<ast::Expression>(nodes[2]));
}

std::unique_ptr<ast::Node> BuildEnumOrFlagLiteral(
    absl::Span<std::unique_ptr<ast::Node>> nodes, ast::EnumLiteral::Kind kind,
    diagnostic::DiagnosticConsumer &) {
  SourceRange span(nodes[0]->span.begin(), nodes[1]->span.end());
  std::vector<std::unique_ptr<ast::Expression>> elems;
  if (auto *stmts = nodes[1]->if_as<Statements>()) {
    // TODO if you want these values to depend on compile-time parameters,
    // you'll need to actually build the AST nodes.
    for (auto &stmt : stmts->content_) {
      ASSERT(stmt, InheritsFrom<ast::Expression>());
      elems.push_back(move_as<ast::Expression>(stmt));
    }
  }

  return std::make_unique<ast::EnumLiteral>(std::move(span), std::move(elems),
                                            kind);
}

std::unique_ptr<ast::Node> BuildScopeLiteral(
    std::unique_ptr<Statements> stmts, SourceRange span,
    diagnostic::DiagnosticConsumer &diag) {
  std::vector<std::unique_ptr<ast::Declaration>> decls;
  for (auto &stmt : stmts->content_) {
    if (stmt->is<ast::Declaration>()) {
      decls.push_back(move_as<ast::Declaration>(stmt));
    } else {
      diag.Consume(diagnostic::Todo{});
    }
  }
  return std::make_unique<ast::ScopeLiteral>(std::move(span), std::move(decls));
}

std::unique_ptr<ast::Node> BuildBlock(std::unique_ptr<Statements> stmts,
                                      diagnostic::DiagnosticConsumer &diag) {
  auto span = stmts->span;  // TODO it's really bigger than this because it
                            // involves the keyword too.

  std::vector<std::unique_ptr<ast::Declaration>> before, after;
  for (auto &stmt : stmts->content_) {
    if (auto *decl = stmt->if_as<ast::Declaration>()) {
      if (decl->id() == "before") {
        before.push_back(move_as<ast::Declaration>(stmt));
      } else if (decl->id() == "after") {
        after.push_back(move_as<ast::Declaration>(stmt));
      } else {
        diag.Consume(diagnostic::Todo{});
      }
    } else {
      diag.Consume(diagnostic::Todo{});
    }
  }
  return std::make_unique<ast::BlockLiteral>(span, std::move(before),
                                             std::move(after));
}

std::unique_ptr<ast::StructLiteral> BuildStructLiteral(
    Statements &&stmts, SourceRange span,
    diagnostic::DiagnosticConsumer &diag) {
  std::vector<std::unique_ptr<ast::Node>> node_stmts =
      std::move(stmts).extract();
  std::vector<ast::Declaration> fields;
  fields.reserve(node_stmts.size());
  for (auto &stmt : node_stmts) {
    if (auto *decl = stmt->if_as<ast::Declaration>()) {
      fields.push_back(std::move(*decl));
    } else {
      diag.Consume(diagnostic::NonDeclarationInStruct{
          .range = stmt->span,
      });
    }
  }

  return std::make_unique<ast::StructLiteral>(std::move(span),
                                              std::move(fields));
}

// TODO rename this now that it supports switch statements too.
std::unique_ptr<ast::Node> BuildParameterizedKeywordScope(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  // TODO should probably not do this with a token but some sort of enumerator
  // so we can ensure coverage/safety.
  ASSERT(nodes[0], InheritsFrom<Token>());
  auto const &tk = nodes[0]->as<Token>().token;
  if (tk == "switch") {
    auto sw   = BuildSwitch(move_as<Statements>(nodes[4]), diag);
    sw->expr_ = move_as<ast::Expression>(nodes[2]);
    return sw;
  } else if (tk == "jump") {
    SourceRange span(nodes.front()->span.begin(), nodes.back()->span.end());
    std::vector<std::unique_ptr<ast::Declaration>> params;
    if (nodes.size() == 5) {
      if (nodes[2]->is<ast::CommaList>()) {
        for (auto &expr : nodes[2]->as<ast::CommaList>().exprs_) {
          ASSERT(expr,
                 InheritsFrom<ast::Declaration>());  // TODO handle failure
          auto decl = move_as<ast::Declaration>(expr);
          decl->flags() |= ast::Declaration::f_IsFnParam;
          params.push_back(std::move(decl));
        }
      } else {
        auto decl = move_as<ast::Declaration>(nodes[2]);
        decl->flags() |= ast::Declaration::f_IsFnParam;
        params.push_back(std::move(decl));
      }
    }

    return std::make_unique<ast::Jump>(
        std::move(span), std::move(params),
        std::move(nodes.back()->as<Statements>()).extract());

  } else if (tk == "struct") {
    diag.Consume(diagnostic::Todo{});
    return nullptr;
  } else {
    UNREACHABLE();
  }
}

std::unique_ptr<ast::Node> BuildConcreteStruct(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  return BuildStructLiteral(
      std::move(nodes[1]->as<Statements>()),
      SourceRange(nodes.front()->span.begin(), nodes.back()->span.end()), diag);
}

std::unique_ptr<ast::Node> BuildKWBlock(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  if (nodes[0]->is<Token>()) {
    std::string const &tk = nodes[0]->as<Token>().token;

    if (bool is_enum = (tk == "enum"); is_enum or tk == "flags") {
      return BuildEnumOrFlagLiteral(std::move(nodes),
                                    is_enum ? ast::EnumLiteral::Kind::Enum
                                            : ast::EnumLiteral::Kind::Flags,
                                    diag);

    } else if (tk == "struct") {
      return BuildConcreteStruct(std::move(nodes), diag);

    } else if (tk == "switch") {
      return BuildSwitch(move_as<Statements>(nodes[1]), diag);

    } else if (tk == "scope") {
      SourceRange span(nodes.front()->span.begin(), nodes.back()->span.end());
      return BuildScopeLiteral(move_as<Statements>(nodes[1]), span, diag);

    } else if (tk == "block") {
      return BuildBlock(move_as<Statements>(nodes[1]), diag);
    } else {
      UNREACHABLE(tk);
    }
  } else {
    UNREACHABLE(nodes[0].get());
  }
}

std::unique_ptr<ast::Node> Parenthesize(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto result            = move_as<ast::Expression>(nodes[1]);
  result->parenthesized_ = true;
  return result;
}

std::unique_ptr<ast::Node> BuildEmptyParen(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  if (nodes[0]->is<ast::Declaration>()) {
    diag.Consume(diagnostic::CallingDeclaration{
        .range = nodes[0]->span,
    });
  }
  SourceRange span(nodes[0]->span.begin(), nodes[2]->span.end());
  return std::make_unique<ast::Call>(std::move(span),
                                     move_as<ast::Expression>(nodes[0]),
                                     core::OrderedFnArgs<ast::Expression>{});
}

template <size_t N>
std::unique_ptr<ast::Node> drop_all_but(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  return std::move(nodes[N]);
}

std::unique_ptr<ast::Node> CombineColonEq(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto *tk_node = &nodes[0]->as<Token>();
  tk_node->token += "=";  // Change : to := and :: to ::=
  tk_node->op = Operator::ColonEq;
  return drop_all_but<0>(std::move(nodes), diag);
}

template <size_t ReturnIndex, size_t... ReservedIndices>
std::unique_ptr<ast::Node> ReservedKeywords(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  (diag.Consume(diagnostic::ReservedKeyword{
       .range   = nodes[ReservedIndices]->span,
       .keyword = nodes[ReservedIndices]->as<Token>().token,
   }),
   ...);
  return MakeInvalidNode(nodes[ReturnIndex]->span);
}

std::unique_ptr<ast::Node> BuildOperatorIdentifier(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto span = nodes[1]->span;
  return std::make_unique<ast::Identifier>(span,
                                           move_as<Token>(nodes[1])->token);
}

std::unique_ptr<ast::Node> LabelScopeNode(
    absl::Span<std::unique_ptr<ast::Node>> nodes,
    diagnostic::DiagnosticConsumer &diag) {
  auto scope_node = move_as<ast::ScopeNode>(nodes[1]);
  if (scope_node->label()) {
    diag.Consume(diagnostic::ScopeNodeAlreadyHasLabel{
        .label_range = scope_node->label()->span,
        .range       = scope_node->span,
    });
  } else {
    scope_node->span = SourceRange(nodes[0]->span.begin(), scope_node->span.end());
    scope_node->set_label(move_as<ast::Label>(nodes[0]));
  }
  return scope_node;
}

constexpr uint64_t OP_B = op_b | comma | colon | eq;
constexpr uint64_t EXPR = expr | fn_expr | scope_expr | fn_call_expr;
// Used in error productions only!
constexpr uint64_t RESERVED = kw_struct | kw_block_head | op_lt;
constexpr uint64_t KW_BLOCK = kw_struct | kw_block_head | kw_block;
// Here are the definitions for all rules in the langugae. For a rule to be
// applied, the node types on the top of the stack must match those given in the
// list (second line of each rule). If so, then the function given in the third
// line of each rule is applied, replacing the matched nodes. Lastly, the new
// nodes type is set to the given type in the first line.
static std::array kRules{
    ParseRule(scope_expr, {fn_call_expr, block_expr}, BuildScopeNode),
    ParseRule(scope_expr, {scope_expr, block_expr}, ExtendScopeNode),
    ParseRule(scope_expr, {scope_expr, expr, scope_expr},
              SugaredExtendScopeNode),
    ParseRule(scope_expr, {label, scope_expr}, LabelScopeNode),
    ParseRule(fn_expr, {EXPR, fn_arrow, EXPR | kw_block}, BuildBinaryOperator),
    ParseRule(expr, {EXPR, (op_bl | OP_B), EXPR}, BuildBinaryOperator),
    ParseRule(op_b, {colon, eq}, CombineColonEq),
    ParseRule(fn_expr, {EXPR, fn_arrow, RESERVED}, ReservedKeywords<1, 2>),
    ParseRule(fn_expr, {RESERVED, fn_arrow, EXPR | kw_block},
              ReservedKeywords<1, 0>),
    ParseRule(fn_expr, {RESERVED, fn_arrow, RESERVED},
              ReservedKeywords<1, 0, 2>),
    ParseRule(expr, {EXPR, (OP_B | yield | op_bl), RESERVED},
              ReservedKeywords<1, 2>),
    ParseRule(expr, {RESERVED, (OP_B | yield | op_bl), RESERVED},
              ReservedKeywords<1, 0, 2>),
    ParseRule(fn_call_expr, {EXPR, l_paren, EXPR, r_paren}, BuildCall),
    ParseRule(fn_call_expr, {EXPR, l_paren, r_paren}, BuildEmptyParen),
    ParseRule(expr, {l_paren, op_l | op_b | eq | op_bl, r_paren},
              BuildOperatorIdentifier),
    ParseRule(expr, {l_paren, r_paren}, BuildEmptyCommaList),
    ParseRule(expr, {EXPR, l_bracket, EXPR, r_bracket}, BuildIndexOperator),
    ParseRule(expr, {l_bracket, r_bracket}, BuildEmptyArray),
    ParseRule(expr, {l_bracket, EXPR, semicolon, EXPR, r_bracket},
              BuildArrayType),
    ParseRule(expr, {l_bracket, EXPR, semicolon, kw_struct, r_bracket},
              BuildGenericStructType),
    ParseRule(expr, {l_bracket, EXPR, semicolon, RESERVED, r_bracket},
              ReservedKeywords<0, 3>),
    ParseRule(expr, {l_bracket, RESERVED, semicolon, EXPR, r_bracket},
              ReservedKeywords<0, 1>),
    ParseRule(expr, {l_bracket, RESERVED, semicolon, RESERVED, r_bracket},
              ReservedKeywords<0, 1, 3>),
    ParseRule(r_paren, {newline, r_paren}, drop_all_but<1>),
    ParseRule(r_bracket, {newline, r_bracket}, drop_all_but<1>),
    ParseRule(r_brace, {newline, r_brace}, drop_all_but<1>),
    ParseRule(l_brace, {newline, l_brace}, drop_all_but<1>),
    ParseRule(stmts, {newline, stmts}, drop_all_but<1>),
    ParseRule(r_paren, {r_paren, newline}, drop_all_but<0>),
    ParseRule(r_bracket, {r_bracket, newline}, drop_all_but<0>),
    ParseRule(r_brace, {r_brace, newline}, drop_all_but<0>),
    // TODO more specifically, the op_b needs to be a '.'
    ParseRule(expr, {expr, op_b, braced_stmts}, BuildDesignatedInitializer),
    ParseRule(braced_stmts, {l_brace, stmts, stmts | EXPR, r_brace},
              BracedStatementsSameLineEnd),
    ParseRule(braced_stmts, {l_brace, stmts, op_lt, r_brace},
              BracedStatementsJumpSameLineEnd),
    ParseRule(braced_stmts, {l_brace, stmts, r_brace}, drop_all_but<1>),
    ParseRule(braced_stmts, {l_brace, r_brace}, EmptyBraces),
    ParseRule(braced_stmts, {l_brace, EXPR, r_brace}, OneBracedStatement),
    ParseRule(braced_stmts, {l_brace, op_lt, r_brace}, OneBracedJump),
    ParseRule(expr, {fn_expr, braced_stmts}, BuildNormalFunctionLiteral),
    ParseRule(expr, {expr, fn_arrow, braced_stmts},
              BuildInferredFunctionLiteral),
    ParseRule(hashtag, {hashtag, newline}, drop_all_but<0>),
    ParseRule(expr, {hashtag, EXPR}, AddHashtag),

    // Call and index operator with reserved words. We can't put reserved words
    // in the first slot because that might conflict with a real use case.
    ParseRule(expr, {EXPR, l_paren, RESERVED, r_paren}, ReservedKeywords<0, 2>),
    ParseRule(expr, {EXPR, l_bracket, RESERVED, r_bracket},
              ReservedKeywords<0, 2>),

    ParseRule(expr, {EXPR, op_r}, BuildRightUnop),
    ParseRule(expr, {(op_l | op_bl | op_lt), EXPR}, BuildLeftUnop),
    ParseRule(stmts, {sop_lt | sop_l, EXPR}, BuildStatementLeftUnop),

    ParseRule(stmts, {yield, EXPR}, BuildUnlabeledYield),
    ParseRule(stmts, {label, yield, EXPR}, BuildLabeledYield),
    ParseRule(stmts, {label, yield}, BuildLabeledYield),

    ParseRule(expr, {RESERVED, (OP_B | yield | op_bl), EXPR},
              ReservedKeywords<1, 0>),
    ParseRule(expr, {l_paren | l_ref, EXPR, r_paren}, Parenthesize),
    ParseRule(expr, {l_bracket, EXPR, r_bracket}, BuildArrayLiteral),
    ParseRule(expr, {l_paren, RESERVED, r_paren}, ReservedKeywords<1, 1>),
    ParseRule(expr, {l_bracket, RESERVED, r_bracket}, ReservedKeywords<1, 1>),
    ParseRule(stmts, {stmts, (EXPR | stmts), newline | eof},
              BuildMoreStatements),
    ParseRule(expr, {kw_struct, l_paren, expr, r_paren, braced_stmts},
              BuildParameterizedKeywordScope),
    ParseRule(expr, {kw_struct, l_paren, r_paren, braced_stmts},
              BuildParameterizedKeywordScope),
    ParseRule(expr, {KW_BLOCK, braced_stmts}, BuildKWBlock),
    ParseRule(expr, {KW_BLOCK, newline}, drop_all_but<0>),

    ParseRule(expr, {(op_l | op_bl | op_lt), RESERVED}, ReservedKeywords<0, 1>),
    // TODO does this rule prevent chained scope blocks on new lines or is it
    // preceeded by a shift rule that eats newlines after a right-brace?
    ParseRule(stmts, {EXPR, (newline | eof)}, BuildOneStatement),
    ParseRule(expr, {l_paren, EXPR, comma, r_paren}, BuildOneElementCommaList),
    ParseRule(comma, {comma, newline}, drop_all_but<0>),
    ParseRule(l_paren, {l_paren, newline}, drop_all_but<0>),
    ParseRule(l_bracket, {l_bracket, newline}, drop_all_but<0>),
    ParseRule(l_brace, {l_brace, newline}, drop_all_but<0>),
    ParseRule(stmts, {stmts, newline}, drop_all_but<0>),

    // TODO also need to handle labels with yields.
    ParseRule(stmts, {op_lt}, BuildControlHandler),
    ParseRule(block_expr, {expr, braced_stmts}, BuildBlockNode),
    ParseRule(eof, {newline, eof}, drop_all_but<1>),
    ParseRule(stmts, {stmts, eof}, drop_all_but<0>),
};

enum class ShiftState : char { NeedMore, EndOfExpr, MustReduce };
struct ParseState {
  // TODO storing the `diag` reference twice is unnecessary.
  explicit ParseState(Source *src, diagnostic::DiagnosticConsumer &diag)
      : lex_state_(src, diag), diag_(diag) {}

  template <size_t N>
  inline Tag get_type() const {
    return tag_stack_[tag_stack_.size() - N];
  }

  template <size_t N>
  inline ast::Node *get() const {
    return node_stack_[node_stack_.size() - N].get();
  }

  ShiftState shift_state() {
    // If the size is just 1, no rule will match so don't bother checking.
    if (node_stack_.size() < 2) { return ShiftState::NeedMore; }

    const auto &ahead = Next();
    if (ahead.tag_ == newline) {
      return brace_count == 0 ? ShiftState::EndOfExpr : ShiftState::MustReduce;
    }

    if (ahead.tag_ == expr and
        (get_type<1>() == fn_call_expr or get_type<1>() == scope_expr)) {
      return ShiftState::NeedMore;
    }

    if (ahead.tag_ == l_brace and (get_type<1>() & kw_block) and
        get_type<2>() == fn_arrow) {
      return ShiftState::MustReduce;
    }

    if (ahead.tag_ == l_brace and get_type<1>() == fn_expr and
        get_type<2>() == fn_arrow) {
      return ShiftState::MustReduce;
    }

    if (ahead.tag_ == l_brace and
        (get_type<1>() & (fn_expr | kw_block_head | kw_struct))) {
      return ShiftState::NeedMore;
    }

    if (get_type<1>() == newline and get_type<2>() == comma) {
      return ShiftState::MustReduce;
    }

    if (get_type<1>() == op_lt and ahead.tag_ != newline) {
      return ShiftState::NeedMore;
    }

    if ((get_type<1>() & (kw_block_head | kw_struct)) and
        ahead.tag_ == newline) {
      return ShiftState::NeedMore;
    }

    if ((get_type<2>() & (kw_block_head | kw_struct)) and
        get_type<1>() == newline) {
      return ShiftState::NeedMore;
    }

    if (ahead.tag_ == r_paren) { return ShiftState::MustReduce; }

    if (get_type<1>() == r_paren and ahead.tag_ == l_brace) {
      size_t i = tag_stack_.size() - 1;
      while (i > 0) {
        if (tag_stack_[i] == fn_arrow) { return ShiftState::MustReduce; }
        if (tag_stack_[i] == stmts) { return ShiftState::NeedMore; }
        --i;
      }
      return ShiftState::NeedMore;
    }

    constexpr uint64_t OP = hashtag | op_r | op_l | op_b | colon | eq | comma |
                            op_bl | op_lt | fn_arrow | yield | sop_l | sop_lt;
    if (get_type<2>() & OP) {
      if (get_type<1>() == r_paren) {
        // TODO this feels like a hack, but maybe this whole function is.
        return ShiftState::MustReduce;
      }
      auto left_prec = precedence(get<2>()->as<Token>().op);
      size_t right_prec;
      if (ahead.tag_ & OP) {
        right_prec = precedence(ahead.node_->as<Token>().op);
      } else if (ahead.tag_ == l_bracket) {
        right_prec = precedence(Operator::Index);

      } else if (ahead.tag_ == l_paren) {
        // TODO this might be a hack. To get the following example to parse
        // correctly:
        //
        //    #tag
        //    (+) ::= ...
        //
        // As it stands we're assuming there's some expression being called
        // between the tag and the paren where the newline actually is. We can
        // get around this here by just explicitly checking that case, but
        // perhaps we should actually just lex "(+)" as it's own symbol with
        // it's own tag type. That might be more robust.
        if (get_type<1>() == newline) { return ShiftState::MustReduce; }
        right_prec = precedence(Operator::Call);
      } else {
        return ShiftState::MustReduce;
      }
      return (left_prec < right_prec) or
                     (left_prec == right_prec and
                      (left_prec & assoc_mask) == right_assoc)
                 ? ShiftState::NeedMore
                 : ShiftState::MustReduce;
    }
    return ShiftState::MustReduce;
  }

  void LookAhead() { lookahead_ = NextToken(&lex_state_); }

  const TaggedNode &Next() {
    if (not lookahead_) { LookAhead(); }
    return *lookahead_;
  }

  std::vector<Tag> tag_stack_;
  std::vector<std::unique_ptr<ast::Node>> node_stack_;
  std::optional<TaggedNode> lookahead_;
  LexState lex_state_;

  // We actually don't care about mathing braces because we are only using this
  // to determine for the REPL if we should prompt for further input. If it's
  // wrong, we won't be able to to parse anyway, so it only needs to be the
  // correct value when the braces match.
  int brace_count = 0;
  diagnostic::DiagnosticConsumer &diag_;
};

// Print out the debug information for the parse stack, and pause.
void Debug(ParseState *ps) {
  // Clear the screen
  fprintf(stderr, "\033[2J\033[1;1H\n");
  for (auto x : ps->tag_stack_) { fprintf(stderr, "%lu, ", x); }
  fprintf(stderr, " -> %lu\n", ps->Next().tag_);

  for (const auto &node_ptr : ps->node_stack_) {
    fputs(node_ptr->DebugString().c_str(), stderr);
  }
  fgetc(stdin);
}

void Shift(ParseState *ps) {
  if (not ps->lookahead_) { ps->LookAhead(); }
  auto ahead     = *std::move(ps->lookahead_);
  ps->lookahead_ = std::nullopt;
  ps->tag_stack_.push_back(ahead.tag_);
  ps->node_stack_.push_back(std::move(ahead.node_));

  auto tag_ahead = ps->Next().tag_;
  if (tag_ahead & (l_paren | l_bracket | l_brace)) {
    ++ps->brace_count;
  } else if (tag_ahead & (r_paren | r_bracket | r_brace)) {
    --ps->brace_count;
  }
}

bool Reduce(ParseState *ps) {
  const ParseRule *matched_rule_ptr = nullptr;
  for (ParseRule const &rule : kRules) {
    if (rule.Match(ps->tag_stack_)) {
      matched_rule_ptr = &rule;
      break;
    }
  }

  // If you make it to the end of the rules and still haven't matched, then
  // return false
  if (matched_rule_ptr == nullptr) { return false; }

  matched_rule_ptr->Apply(&ps->node_stack_, &ps->tag_stack_, ps->diag_);

  return true;
}

void CleanUpReduction(ParseState *state) {
  // Reduce what you can
  while (Reduce(state)) {
    if (debug::parser) { Debug(state); }
  }

  Shift(state);

  // Reduce what you can again
  while (Reduce(state)) {
    if (debug::parser) { Debug(state); }
  }
  if (debug::parser) { Debug(state); }
}
}  // namespace

std::vector<std::unique_ptr<ast::Node>> Parse(
    Source *src, diagnostic::DiagnosticConsumer &diag) {
  ParseState state(src, diag);
  Shift(&state);

  while (state.Next().tag_ != eof) {
    ASSERT(state.tag_stack_.size() == state.node_stack_.size());
    // Shift if you are supposed to, or if you are unable to reduce.
    if (state.shift_state() == ShiftState::NeedMore or not Reduce(&state)) {
      Shift(&state);
    }

    if (debug::parser) { Debug(&state); }
  }

  // Cleanup
  CleanUpReduction(&state);

  // end()
  switch (state.node_stack_.size()) {
    case 0: UNREACHABLE();
    case 1:
      // TODO log an error
      if (state.tag_stack_.back() & (eof | bof)) { return {}; }
      return std::move(move_as<Statements>(state.node_stack_.back())->content_);

    default: {
      std::vector<SourceRange> lines;

      for (size_t i = 0; i < state.node_stack_.size(); ++i) {
        if (state.tag_stack_[i] &
            (braced_stmts | l_paren | r_paren | l_bracket | r_bracket |
             l_brace | r_brace | semicolon | fn_arrow | expr)) {
          lines.push_back(state.node_stack_[i]->span);
        }
      }

      // This is an exceedingly crappy error message.
      diag.Consume(diagnostic::UnknownParseError{
          .lines = std::move(lines),
      });
      return {};
    }
  }
}

}  // namespace frontend
