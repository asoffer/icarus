#ifndef ICARUS_PARSER_H
#define ICARUS_PARSER_H

#include <memory>
#include <iostream>

#include "Lexer.h"
#include "Rule.h"
#include "AST.h"
#include "typedefs.h"
#include "Language.h"

class Parser {
  public:

    Parser(const std::string& filename);

    AST::Node *parse();

  private:
    bool should_shift();
    void shift();
    bool reduce();

    void show_debug() const;

    NPtrVec2 stack_;
    std::unique_ptr<AST::TokenNode> lookahead_;
    Lexer lexer_;
};

inline void Parser::shift() {
  std::unique_ptr<AST::TokenNode> next_node_ptr(new AST::TokenNode);
  lexer_ >> *next_node_ptr;

  // Never shift comments onto the stack
  if (next_node_ptr->node_type() == Language::comment) {

    shift();
    return;
  }

  stack_.push_back(lookahead_.release());
  lookahead_ = std::move(next_node_ptr);
}

#endif  // ICARUS_PARSER_H
