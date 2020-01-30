#ifndef ICARUS_FRONTEND_LEX_LEXEME_H
#define ICARUS_FRONTEND_LEX_LEXEME_H

#include <iostream>

#include "ast/node.h"
#include "base/meta.h"
#include "frontend/lex/operators.h"
#include "frontend/lex/syntax.h"
#include "frontend/lex/tag.h"

namespace frontend {

// Represents the results that are produced by the lexer. This is either an
// operator, syntactic information, or an AST node.
struct Lexeme {
  explicit Lexeme(std::unique_ptr<ast::Node>&& n)
      : value_(std::move(n)),
        span_(std::get<std::unique_ptr<ast::Node>>(value_)->span) {}
  explicit Lexeme(Operator op, SourceRange const& span)
      : value_(op), span_(span) {}
  explicit Lexeme(Syntax s, SourceRange const& span) : value_(s), span_(span) {}
  explicit Lexeme(ast::Hashtag h, SourceRange const& span)
      : value_(h), span_(span) {}

  constexpr Operator op() const { return std::get<Operator>(value_); }

  std::variant<std::unique_ptr<ast::Node>, Operator, Syntax, ast::Hashtag>
  get() && {
    return std::move(value_);
  }

  constexpr Tag tag() const {
    return std::visit(
        [](auto&& x) {
          using T = std::decay_t<decltype(x)>;
          if constexpr (std::is_same_v<T, Syntax>) {
            return TagFrom(x);
          } else if constexpr (std::is_same_v<T, Operator>) {
            return TagFrom(x);
          } else if constexpr (std::is_same_v<T, std::unique_ptr<ast::Node>>) {
            return expr;
          } else if constexpr (std::is_same_v<T, ast::Hashtag>) {
            return hashtag;
          } else {
            static_assert(base::always_false<T>());
          }
        },
        value_);
  }

  SourceRange span() const { return span_; }

 private:
  std::variant<std::unique_ptr<ast::Node>, Operator, Syntax, ast::Hashtag>
      value_;
  SourceRange span_;
};

}  // namespace frontend

#endif  // ICARUS_FRONTEND_LEX_LEX_H
