#include "parse/parser.h"

#include <concepts>
#include <cstring>
#include <span>

#include "common/debug.h"
#include "diagnostics/consumer/consumer.h"
#include "nth/debug/debug.h"
#include "parse/declaration.h"
#include "parse/node_index.h"
#include "parse/precedence.h"
#include "parse/tree.h"

namespace ic {
namespace {

struct Parser {
  explicit Parser(TokenBuffer const& token_buffer, ScopeTree& scope_tree,
                  diag::DiagnosticConsumer& diagnostic_consumer)
      : iterator_(token_buffer.begin()),
        token_buffer_(token_buffer),
        scope_tree_(scope_tree),
        diagnostic_consumer_(diagnostic_consumer) {}

  Token current_token() const {
    NTH_REQUIRE((v.debug), iterator_ != token_buffer_.end());
    return *iterator_;
  }

  void ForceCompleteParsing() { state_.clear(); }

  struct State {
    enum class Kind {
#define IC_XMACRO_PARSER_STATE(state) state,
#include "parse/state.xmacro.h"
    };

    friend void NthPrint(auto& p, auto&, Kind const& k) {
      switch (k) {
#define IC_XMACRO_PARSER_STATE(kind)                                           \
  case Kind::kind:                                                             \
    p.write(#kind);                                                            \
    break;
#include "parse/state.xmacro.h"
      }
    }

    Kind kind;
    Precedence ambient_precedence = Precedence::Loosest();
    Token token;
    uint32_t subtree_start = -1;

    friend void NthPrint(auto& p, auto& f, State const& s) {
      nth::Interpolate<"[{} {}]">(p, f, s.kind, s.subtree_start);
    }
  };

#define IC_XMACRO_PARSER_STATE(state) void Handle##state(ParseTree& tree);
#include "parse/state.xmacro.h"

  std::span<State const> state() const { return state_; }
  std::span<State> state() { return state_; }

  void pop_and_discard_state() {
    NTH_REQUIRE((v.debug), not state_.empty());
    state_.pop_back();
  }

  void push_state(State state) { state_.push_back(state); }

  State pop_state() {
    NTH_REQUIRE((v.debug), not state_.empty());
    auto state = state_.back();
    pop_and_discard_state();
    return state;
  }

  void IgnoreAnyNewlines() {
    while (current_token().kind() != Token::Kind::Eof and
           current_token().kind() == Token::Kind::Newline) {
      ++iterator_;
    }
  }

  State Expression(ParseTree const& tree,
                   Precedence p = Precedence::Loosest()) const {
    return {.kind               = State::Kind::Expression,
            .ambient_precedence = p,
            .subtree_start      = tree.size()};
  };

  void ExpandState(auto... states) {
    auto state = pop_state();
    int dummy;
    (dummy = ... = (ExpandStateImpl(state, states), 0));
  }

 private:
  void HandleParenthesizedCommaSeparatedSequenceOf(State::Kind one_state,
                                                   State::Kind repeat_state,
                                                   ParseTree& tree) {
    IgnoreAnyNewlines();
    if (current_token().kind() == Token::Kind::RightParen) {
      pop_and_discard_state();
      return;
    } else if (current_token().kind() == Token::Kind::Comma) {
      ++iterator_;
      IgnoreAnyNewlines();
      ExpandState(State{.kind = one_state, .subtree_start = tree.size()},
                  repeat_state);
    } else {
      NTH_UNREACHABLE("{}") <<= {current_token().kind()};
    }
  }

  DeclarationKind& PushDeclaration() {
    auto& k = declaration_kinds_.emplace_back();
    k.set_parameter(inside_function_declaration());
    return k;
  }

  DeclarationKind& CurrentDeclaration() {
    NTH_REQUIRE(not declaration_kinds_.empty());
    return declaration_kinds_.back();
  }

  DeclarationKind PopDeclaration() {
    NTH_REQUIRE(not declaration_kinds_.empty());
    auto result = declaration_kinds_.back();
    declaration_kinds_.pop_back();
    return result;
  }

  Scope::Index PushScope() {
    NTH_REQUIRE(not scope_indices_.empty());
    return scope_indices_.emplace_back(
        scope_tree_.insert_child(scope_indices_.back()));
  }

  void PopScope() {
    NTH_REQUIRE(not scope_indices_.empty());
    scope_indices_.pop_back();
  }

  void ExpandStateImpl(State prototype, State::Kind kind) {
    prototype.kind = kind;
    ExpandStateImpl(prototype, prototype);
  }
  void ExpandStateImpl(State const&, State state) { state_.push_back(state); }

  std::vector<State> state_ = {
      {.kind = State::Kind::Module, .subtree_start = 0},
      {.kind = State::Kind::Newlines, .subtree_start = 0},
  };

  bool inside_function_declaration() const {
    return inside_function_declaration_.back();
  }

  bool pop_inside_function_decl() {
    bool b = inside_function_declaration_.back();
    inside_function_declaration_.pop_back();
    return b;
  }

  void push_inside_function_decl(bool b) {
    inside_function_declaration_.push_back(b);
  }

  TokenBuffer::const_iterator iterator_;

  TokenBuffer const& token_buffer_;
  ScopeTree& scope_tree_;
  std::vector<bool> inside_function_declaration_ = {false};
  std::vector<DeclarationKind> declaration_kinds_;
  std::vector<Scope::Index> scope_indices_ = {Scope::Index::Root()};
  diag::DiagnosticConsumer& diagnostic_consumer_;
};

struct DebugView {
  friend void NthPrint(auto& p, auto& f, DebugView const view) {
    for (auto iter = view.state.rbegin(); iter != view.state.rend(); ++iter) {
      nth::Interpolate<"    {}\n">(p, f, *iter);
    }
  }
  std::span<Parser::State const> state;
};

}  // namespace

ParseResult Parse(TokenBuffer const& token_buffer,
                  diag::DiagnosticConsumer& diagnostic_consumer) {
  ParseResult result;
  Parser p(token_buffer, result.scope_tree, diagnostic_consumer);

  while (not p.state().empty()) {
    if (result.parse_tree.size() != 0) {
      NTH_REQUIRE((v.debug), result.parse_tree.back().subtree_size > 0);
    }
    NTH_LOG((v.when(debug::parser)), "{} {}:\n{}") <<=
        {p.current_token(), p.state().back().subtree_start,
         DebugView{.state = p.state()}};
    switch (p.state().back().kind) {
#define IC_XMACRO_PARSER_STATE(state)                                          \
  case Parser::State::Kind::state:                                             \
    p.Handle##state(result.parse_tree);                                        \
    break;
#include "parse/state.xmacro.h"
    }
  }
  return result;
}

void Parser::HandleNewlines(ParseTree& tree) {
  pop_and_discard_state();
  IgnoreAnyNewlines();
}

void Parser::HandleModule(ParseTree& tree) {
  ExpandState(State{
      .kind               = State::Kind::StatementSequence,
      .ambient_precedence = Precedence::Loosest(),
      .token              = *iterator_,
      .subtree_start      = tree.size(),
  });
}

void Parser::HandleDeclaration(ParseTree& tree) {
  switch (current_token().kind()) {
    case Token::Kind::Let:
      PushDeclaration();
      ExpandState(State::Kind::DeclaredSymbol,
                  State::Kind::ColonToEndOfDeclaration,
                  State{
                      .kind          = State::Kind::ResolveDeclaration,
                      .subtree_start = tree.size(),
                  });
      tree.append_leaf(ParseNode::Kind::DeclarationStart, *++iterator_);
      break;
    case Token::Kind::Var:
      PushDeclaration();
      ExpandState(State::Kind::DeclaredSymbol,
                  State::Kind::ColonToEndOfDeclaration,
                  State{
                      .kind          = State::Kind::ResolveDeclaration,
                      .subtree_start = tree.size(),
                  });
      tree.append_leaf(ParseNode::Kind::DeclarationStart, *++iterator_);
      break;
    default:
      diagnostic_consumer_.Consume({
          diag::Header(diag::MessageKind::Error),
          diag::Text("Parsing error. Expected a declaration, which must start "
                     "with either `let` or `var`."),
      });
      ForceCompleteParsing();
  }
}

void Parser::HandleColonToEndOfDeclaration(ParseTree& tree) {
  ParseNode::Kind node_kind;
  switch (current_token().kind()) {
    case Token::Kind::ColonColonEqual: {
      auto& cd = CurrentDeclaration();
      cd.set_explicit_type(false);
      cd.set_constant(true);
      cd.set_initializer(true);
      ExpandState(Expression(tree));
    } break;
    case Token::Kind::ColonEqual: {
      auto& cd = CurrentDeclaration();
      cd.set_explicit_type(false);
      cd.set_constant(false);
      cd.set_initializer(true);
      ExpandState(Expression(tree));
    } break;
    case Token::Kind::ColonColon: {
      auto& cd = CurrentDeclaration();
      cd.set_explicit_type(true);
      cd.set_constant(true);
      ExpandState(Expression(tree),
                  State{
                      .kind = State::Kind::ResolveUninferredTypeDeclaration,
                      .subtree_start = state().back().subtree_start,
                  });
    } break;
    case Token::Kind::Colon: {
      auto& cd = CurrentDeclaration();
      cd.set_explicit_type(true);
      cd.set_constant(false);
      ExpandState(Expression(tree),
                  State{
                      .kind = State::Kind::ResolveUninferredTypeDeclaration,
                      .subtree_start = state().back().subtree_start,
                  });
    } break;
    default: NTH_UNIMPLEMENTED("{}") <<= {current_token().kind()};
  }
  ++iterator_;
}

void Parser::HandleStatement(ParseTree& tree) {
  tree.append_leaf(ParseNode::Kind::StatementStart, Token::Invalid());
  switch (current_token().kind()) {
    case Token::Kind::Let:
      PushDeclaration();
      ExpandState(State::Kind::DeclaredSymbol,
                  State::Kind::ColonToEndOfDeclaration,
                  State{
                      .kind          = State::Kind::ResolveDeclaration,
                      .subtree_start = tree.size(),
                  },
                  State{
                      .kind          = State::Kind::ResolveStatement,
                      .subtree_start = tree.size() - 1,
                  });
      tree.append_leaf(ParseNode::Kind::DeclarationStart, *++iterator_);
      break;
    case Token::Kind::Var:
      PushDeclaration();
      ExpandState(State::Kind::DeclaredSymbol,
                  State::Kind::ColonToEndOfDeclaration,
                  State{
                      .kind          = State::Kind::ResolveDeclaration,
                      .subtree_start = tree.size(),
                  },
                  State{
                      .kind          = State::Kind::ResolveStatement,
                      .subtree_start = tree.size() - 1,
                  });
      tree.append_leaf(ParseNode::Kind::DeclarationStart, *++iterator_);
      break;
    case Token::Kind::Return:
      tree.back().statement_kind = ParseNode::StatementKind::Return;
      ExpandState(State::Kind::Expression,
                  State{
                      .kind          = State::Kind::ResolveReturn,
                      .subtree_start = tree.size(),
                  },
                  State{
                      .kind          = State::Kind::ResolveStatement,
                      .subtree_start = tree.size() - 1,
                  });
      ++iterator_;
      break;
    case Token::Kind::If:
      tree.back().statement_kind = ParseNode::StatementKind::Expression;
      ExpandState(
          State{
              .kind          = State::Kind::ParenthesizedExpression,
              .subtree_start = tree.size(),
          },
          State{
              .kind          = State::Kind::IfStatementTrueBranchStart,
              .subtree_start = tree.size(),
          },
          State{
              .kind          = State::Kind::BracedStatementSequence,
              .subtree_start = tree.size(),
          },
          State{
              .kind               = State::Kind::ResolveIfStatement,
              .ambient_precedence = Precedence::Loosest(),
              .token              = *iterator_,
              .subtree_start      = tree.size(),
          },
          State{
              .kind          = State::Kind::ResolveStatement,
              .subtree_start = tree.size() - 1,
          });
      ++iterator_;
      return;
    default:
      tree.back().statement_kind = ParseNode::StatementKind::Expression;
      ExpandState(Expression(tree), State{
                                        .kind = State::Kind::ResolveStatement,
                                        .subtree_start = tree.size() - 1,
                                    });
      break;
  }
}

void Parser::HandleIfStatementTrueBranchStart(ParseTree& tree) {
  tree.append_leaf(ParseNode::Kind::IfStatementTrueBranchStart, *iterator_);
  tree.back().scope_index = PushScope();
  pop_and_discard_state();
}

void Parser::HandleResolveIfStatement(ParseTree& tree) {
  PopScope();
  auto state = pop_state();
  tree.append(ParseNode::Kind::IfStatement, state.token, state.subtree_start);
}

void Parser::HandleParenthesizedExpression(ParseTree& tree) {
  if (current_token().kind() == Token::Kind::LeftParen) {
    ++iterator_;
    IgnoreAnyNewlines();
    ExpandState(Expression(tree), State::Kind::ClosingParenthesis);
  } else {
    NTH_UNIMPLEMENTED();
  }
}

void Parser::HandleBracedStatementSequence(ParseTree& tree) {
  if (current_token().kind() == Token::Kind::LeftBrace) {
    ++iterator_;
    IgnoreAnyNewlines();
    ExpandState(
        State{
            .kind               = State::Kind::StatementSequence,
            .ambient_precedence = Precedence::Loosest(),
            .subtree_start      = tree.size(),
        },
        State::Kind::ClosingBrace);
  } else {
    NTH_UNIMPLEMENTED();
  }
}

void Parser::HandleResolveStatement(ParseTree& tree) {
  State state = pop_state();
  tree.append(ParseNode::Kind::Statement, Token::Invalid(),
              state.subtree_start);
}

void Parser::HandleStatementSequence(ParseTree& tree) {
  tree.append_leaf(ParseNode::Kind::ScopeStart, Token::Invalid());
  if (current_token().kind() == Token::Kind::Eof or
      current_token().kind() == Token::Kind::RightBrace) {
    ExpandState(State::Kind::ResolveStatementSequence);
    return;
  }

  ExpandState(State::Kind::Statement, State::Kind::Newlines,
              State::Kind::SubsequentStatementSequence,
              State::Kind::ResolveStatementSequence);
}

void Parser::HandleSubsequentStatementSequence(ParseTree& tree) {
  if (current_token().kind() == Token::Kind::Eof or
      current_token().kind() == Token::Kind::RightBrace) {
    pop_and_discard_state();
    return;
  }

  ExpandState(State::Kind::Statement, State::Kind::Newlines,
              State::Kind::SubsequentStatementSequence);
}

void Parser::HandleResolveStatementSequence(ParseTree& tree) {
  State state = pop_state();
  auto& start = tree[ParseNodeIndex(state.subtree_start)];
  NTH_REQUIRE(start.kind == ParseNode::Kind::ScopeStart);
  start.corresponding_statement_sequence = ParseNodeIndex(tree.size());
  tree.append(ParseNode::Kind::StatementSequence, Token::Invalid(),
              state.subtree_start);
}

void Parser::HandleResolveUninferredTypeDeclaration(ParseTree& tree) {
  switch (current_token().kind()) {
    case Token::Kind::Equal:
      ++iterator_;
      CurrentDeclaration().set_initializer(true);
      ExpandState(Expression(tree));
      break;
    default:
      CurrentDeclaration().set_initializer(false);
      pop_and_discard_state();
      break;
  }
}

void Parser::HandleResolveDeclaration(ParseTree& tree) {
  State state = pop_state();
  tree.append(ParseNode::Kind::Declaration, state.token, state.subtree_start);
  ParseNodeIndex decl_index(tree.size() - 1);
  auto start_index = tree.subtree_range(decl_index).lower_bound();
  tree[start_index].declaration_info = {
      .index = decl_index,
      .kind  = PopDeclaration(),
  };
}

void Parser::HandleDeclaredSymbol(ParseTree& tree) {
  NTH_REQUIRE((v.debug), current_token().kind() == Token::Kind::Identifier);
  state()[state().size() - 4].token = current_token();
  tree.append_leaf(ParseNode::Kind::DeclaredIdentifier, *iterator_++);
  pop_and_discard_state();
}

void Parser::HandleExpression(ParseTree& tree) {
  switch (current_token().kind()) {
    case Token::Kind::Fn:
      push_inside_function_decl(true);
      tree.append_leaf(ParseNode::Kind::FunctionLiteralStart, *iterator_++);
      tree.back().scope_index = PushScope();
      if (iterator_->kind() != Token::Kind::LeftParen) { NTH_UNIMPLEMENTED(); }
      ++iterator_;
      if (iterator_->kind() == Token::Kind::RightParen) {
        ExpandState(
            State{
                .kind = State::Kind::FunctionLiteralReturnTypeStart,
                .subtree_start = tree.size(),
            },
            State{
                .kind = State::Kind::FunctionLiteralBody,
                .subtree_start = tree.size(),
            },
            State::Kind::ResolveFunctionLiteral);
      } else {
        ExpandState(
            State{
                .kind          = State::Kind::Declaration,
                .subtree_start = tree.size(),
            },
            State{
                .kind          = State::Kind::CommaSeparatedDeclarationSequence,
                .subtree_start = tree.size(),
            },
            State{
                .kind          = State::Kind::FunctionLiteralReturnTypeStart,
                .subtree_start = tree.size(),
            },
            State{
                .kind          = State::Kind::FunctionLiteralBody,
                .subtree_start = tree.size(),
            },
            State::Kind::ResolveFunctionLiteral);
      }
      break;
    case Token::Kind::Star:
      ++iterator_;
      ExpandState(Expression(tree, Precedence::TightUnary()),
                  State::Kind::ResolvePointerType);
      return;
    case Token::Kind::BracketedStar:
      ++iterator_;
      ExpandState(Expression(tree, Precedence::TightUnary()),
                  State::Kind::ResolveBufferPointerType);
      return;
    case Token::Kind::Backslash:
      ++iterator_;
      ExpandState(Expression(tree, Precedence::TightUnary()),
                  State::Kind::ResolveSliceType);
      return;
    default:
      ExpandState(State::Kind::AtomicTerm, State::Kind::ExpressionSuffix);
  }
}

void Parser::HandleClosingBrace(ParseTree& tree) {
  IgnoreAnyNewlines();
  if (current_token().kind() == Token::Kind::RightBrace) {
    ++iterator_;
    pop_and_discard_state();
  } else {
    NTH_UNIMPLEMENTED();
  }
}

void Parser::HandleClosingParenthesis(ParseTree& tree) {
  IgnoreAnyNewlines();
  if (current_token().kind() == Token::Kind::RightParen) {
    ++iterator_;
    pop_and_discard_state();
  } else {
    NTH_UNIMPLEMENTED();
  }
}

void Parser::HandleInvocationArgumentSequence(ParseTree& tree) {
  IgnoreAnyNewlines();
  if (current_token().kind() == Token::Kind::RightParen) {
    ExpandState(State::Kind::ResolveInvocationArgumentSequence);
    return;
  } else if (current_token().kind() == Token::Kind::Comma) {
    ++iterator_;
    IgnoreAnyNewlines();
    ExpandState(Expression(tree), State::Kind::InvocationArgumentSequence);
  } else {
    NTH_UNREACHABLE("{}") <<= {current_token().kind()};
  }
}

void Parser::HandleCommaSeparatedExpressionSequence(ParseTree& tree) {
  HandleParenthesizedCommaSeparatedSequenceOf(
      State::Kind::Expression, State::Kind::CommaSeparatedExpressionSequence,
      tree);
}

void Parser::HandleCommaSeparatedDeclarationSequence(ParseTree& tree) {
  HandleParenthesizedCommaSeparatedSequenceOf(
      State::Kind::Declaration, State::Kind::CommaSeparatedDeclarationSequence,
      tree);
}

void Parser::HandleResolveFunctionTypeParameters(ParseTree& tree) {
  tree.append(ParseNode::Kind::FunctionTypeParameters, current_token(),
              state().back().subtree_start);
  tree.set_back_child_count();
  ++iterator_;
  pop_and_discard_state();
}

void Parser::HandleResolveFunctionLiteral(ParseTree& tree) {
  tree.append(ParseNode::Kind::FunctionLiteral, current_token(),
              state().back().subtree_start);
  pop_and_discard_state();
}

void Parser::HandleFunctionLiteralReturnTypeStart(ParseTree& tree) {
  NTH_REQUIRE((v.debug), current_token().kind() == Token::Kind::RightParen);
  ++iterator_;
  if (current_token().kind() != Token::Kind::MinusGreater) {
    NTH_UNIMPLEMENTED();
  }

  ++iterator_;
  ExpandState(State::Kind::Expression);
}

void Parser::HandleFunctionLiteralBody(ParseTree& tree) {
  pop_inside_function_decl();
  tree.append(ParseNode::Kind::FunctionLiteralSignature, Token::Invalid(),
              state().back().subtree_start);
  ExpandState(State{
      .kind               = State::Kind::BracedStatementSequence,
      .ambient_precedence = Precedence::Loosest(),
      .token              = *iterator_,
      .subtree_start      = tree.size(),
  });
}

void Parser::HandleResolveMemberTerm(ParseTree& tree) {
  if (current_token().kind() != Token::Kind::Identifier) {
    NTH_UNIMPLEMENTED("{}") <<= {current_token()};
  }
  tree.append(ParseNode::Kind::MemberExpression, current_token(),
              state().back().subtree_start);
  ++iterator_;
  pop_and_discard_state();
}

void Parser::HandleAtomicTerm(ParseTree& tree) {
  ParseNode::Kind k;
  switch (current_token().kind()) {
    case Token::Kind::LeftParen: {
      size_t paren_gap      = iterator_->payload();
      auto kind_after_paren = token_buffer_[paren_gap + 1].kind();
      if (kind_after_paren == Token::Kind::MinusGreater) {
        ++iterator_;
        if (std::distance(token_buffer_.begin(), iterator_) == paren_gap) {
          ExpandState(State::Kind::ResolveFunctionTypeParameters);
        } else {
          ExpandState(State::Kind::Expression,
                      State::Kind::CommaSeparatedExpressionSequence,
                      State::Kind::ResolveFunctionTypeParameters);
        }
        return;
      } else {
        NTH_UNIMPLEMENTED();
      }
    } break;
    case Token::Kind::Import:
      ++iterator_;
      ExpandState(State::Kind::Expression, State::Kind::ResolveImport);
      return;
#define IC_XMACRO_ATOM(token_kind, parse_node_kind)                            \
  case Token::Kind::token_kind:                                                \
    k = ParseNode::Kind::parse_node_kind;                                      \
    break;
#include "common/language/atoms.xmacro.h"
    default: NTH_UNIMPLEMENTED("Token: {}") <<= {current_token()};
  }

  tree.append_leaf(k, *iterator_++);
  pop_and_discard_state();
}

void Parser::HandleResolveImport(ParseTree& tree) {
  tree.append(ParseNode::Kind::Import, current_token(),
              state().back().subtree_start);
  pop_and_discard_state();
  ++iterator_;
}


void Parser::HandleResolveReturn(ParseTree& tree) {
  tree.append(ParseNode::Kind::Return, current_token(),
              state().back().subtree_start);
  pop_and_discard_state();
}

void Parser::HandleResolveAssignment(ParseTree& tree) {
  // TODO: It'd be nice if this was just a category on statements.
  auto s = pop_state();
  tree[ParseNodeIndex(s.subtree_start - 1)].statement_kind =
      ParseNode::StatementKind::Assignment;
  // TODO: s.subtree_start - 1 seems like a bit of a hack and I'm not entirely
  // sure it's correct.
  tree.append(ParseNode::Kind::Assignment, s.token, s.subtree_start);
}

void Parser::HandleResolveInvocationArgumentSequence(ParseTree& tree) {
  NTH_REQUIRE(current_token().kind() == Token::Kind::RightParen);
  tree.append(ParseNode::Kind::CallExpression, current_token(),
              state().back().subtree_start);
  tree.set_back_child_count();
  ++iterator_;
  pop_and_discard_state();
}

void Parser::HandleExpressionSuffix(ParseTree& tree) {
  Precedence p = Precedence::Loosest();
  switch (current_token().kind()) {
    case Token::Kind::Newline:
    case Token::Kind::Eof: pop_and_discard_state(); return;
    case Token::Kind::Equal:
      ++iterator_;
      tree.append(ParseNode::Kind::AssignedValueStart, current_token(),
                  tree.size());
      ExpandState(State::Kind::Expression, State::Kind::ResolveAssignment);
      return;
#define IC_XMACRO_TOKEN_KIND_BINARY_OPERATOR(kind, symbol, precedence_group)   \
  case Token::Kind::kind:                                                      \
    p = Precedence::precedence_group();                                        \
    break;

#include "lexer/token_kind.xmacro.h"
    case Token::Kind::Star: p = Precedence::MultiplyDivide(); break;
    case Token::Kind::LeftParen: {
      ++iterator_;
      IgnoreAnyNewlines();
      tree.append(ParseNode::Kind::InvocationArgumentStart, current_token(),
                  tree.size());
      if (current_token().kind() == Token::Kind::RightParen) {
        ExpandState(State::Kind::ResolveInvocationArgumentSequence,
                    State::Kind::ExpressionSuffix);
      } else {
        ExpandState(State{.kind          = State::Kind::Expression,
                          .subtree_start = tree.size()},
                    State::Kind::InvocationArgumentSequence,
                    State::Kind::ExpressionSuffix);
      }
      return;
    } break;
    case Token::Kind::Period: {
      ++iterator_;
      ExpandState(
          State{.kind          = State::Kind::ResolveMemberTerm,
                .subtree_start = tree.size() - tree.back().subtree_size},
          State{.kind          = State::Kind::ExpressionSuffix,
                .subtree_start = tree.size() - tree.back().subtree_size});
      return;
    } break;
    default: pop_and_discard_state(); return;
  }
  uint32_t subtree_start;
  State state = pop_state();
  switch (Precedence::Priority(state.ambient_precedence, p)) {
    case Priority::Left:
      if (state_.back().kind == State::Kind::ResolveExpressionGroup) {
        tree.append(ParseNode::Kind::ExpressionPrecedenceGroup,
                    Token::Invalid(), state_.back().subtree_start);
        tree.set_back_child_count();
        subtree_start = state.subtree_start;
      } else {
        auto next_state = state_.back();
        state.ambient_precedence =
            std::next(this->state().rbegin())->ambient_precedence;
        ExpandState(next_state, state);
        return;
      }
      break;
    case Priority::Same:
      NTH_REQUIRE(state_.back().kind == State::Kind::ResolveExpressionGroup);
      subtree_start = state.subtree_start;
      break;
    case Priority::Right:
      push_state({.kind          = State::Kind::ResolveExpressionGroup,
                  .token         = Token::Invalid(),
                  .subtree_start = state.subtree_start});
      // Plus one because of the infix operator we're going to append.
      subtree_start = tree.size() + 1;
      break;
    case Priority::Ambiguous:
      NTH_UNIMPLEMENTED(
          "Ambiguous precedences Precedence::{} vs Precedence::{}") <<=
          {state.ambient_precedence, p};
      break;
  }

  tree.append_leaf(ParseNode::Kind::InfixOperator, *iterator_++);
  push_state(State{
      .kind               = State::Kind::Expression,
      .ambient_precedence = p,
      .subtree_start      = subtree_start,
  });
}

void Parser::HandleResolveExpressionGroup(ParseTree& tree) {
  auto state = pop_state();
  tree.append(ParseNode::Kind::ExpressionPrecedenceGroup, Token::Invalid(),
              state.subtree_start);
  tree.set_back_child_count();
}

void Parser::HandleResolvePointerType(ParseTree& tree) {
  auto state = pop_state();
  tree.append(ParseNode::Kind::Pointer, Token::Invalid(), state.subtree_start);
}

void Parser::HandleResolveBufferPointerType(ParseTree& tree) {
  auto state = pop_state();
  tree.append(ParseNode::Kind::BufferPointer, Token::Invalid(),
              state.subtree_start);
}

void Parser::HandleResolveSliceType(ParseTree& tree) {
  auto state = pop_state();
  tree.append(ParseNode::Kind::Slice, Token::Invalid(), state.subtree_start);
}

}  // namespace ic
