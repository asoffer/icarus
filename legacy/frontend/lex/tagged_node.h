#ifndef ICARUS_FRONTEND_LEX_TAGGED_NODE_H
#define ICARUS_FRONTEND_LEX_TAGGED_NODE_H

#include <memory>
#include <utility>

#include "frontend/lex/lexeme.h"
#include "frontend/lex/tag.h"
#include "frontend/lex/token.h"
#include "nth/meta/sequence.h"
#include "nth/meta/type.h"

namespace frontend {

struct TaggedNode {
  std::unique_ptr<ast::Node> node_;
  Tag tag_{};

  TaggedNode() = default;

  /* implicit */ TaggedNode(Lexeme &&l) : tag_(l.tag()) {
    auto range = l.range();
    std::visit(
        [&](auto &&x) {
          constexpr auto type = nth::type<std::decay_t<decltype(x)>>;
          if constexpr (type == nth::type<Syntax>) {
            node_ = std::make_unique<Token>(range, false);
          } else if constexpr (type == nth::type<Operator>) {
            node_ = std::make_unique<Token>(range, false);
          } else if constexpr (type == nth::type<std::unique_ptr<ast::Node>>) {
            node_ = std::move(x);
          } else {
            node_ = std::make_unique<Token>(range, true);
          }
        },
        std::move(l).get());
  }

  TaggedNode(std::unique_ptr<ast::Node> node, Tag tag)
      : node_(std::move(node)), tag_(tag) {}

  TaggedNode(std::string_view range, const std::string &token, Tag tag);

  bool valid() const { return node_ != nullptr; }
  static TaggedNode Invalid() { return TaggedNode{}; }
};
}  // namespace frontend

#endif  // ICARUS_FRONTEND_LEX_TAGGED_NODE_H