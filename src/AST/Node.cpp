#include <map>
#include "AST/Node.h"

namespace AST {

  std::map<Node::Type, std::string> Node::debug_map = {
    { Node::unknown, "Unknown" },
    { Node::eof, "EOF" },
    { Node::newline, "Newline" },
    { Node::comment, "Comment" },
    { Node::identifier, "Identifier" },
    { Node::integer, "Integer" },
    { Node::real, "Real" },
    { Node::string_literal, "String" },
    { Node::generic_operator, "Operator" },
    { Node::declaration, ":" },
    { Node::assignment, "=" },
    { Node::key_value_joiner, "=>" },
    { Node::key_value_pair, "( => )" },
    { Node::expression, "Expression" },
    { Node::Type::left_paren, "Left Paren" },
    { Node::Type::right_paren, "Right Paren" },
    { Node::Type::left_brace, "Left Brace" },
    { Node::Type::right_brace, "Right Brace" },
    { Node::Type::left_bracket, "Left Bracket" },
    { Node::Type::right_bracket, "Right Bracket" },
    { Node::reserved_if, "If" },
    { Node::reserved_else, "Else" },
    { Node::reserved_case, "Case" },
    { Node::reserved_loop, "Loop" },
    { Node::reserved_while, "While" },
    { Node::reserved_break, "Break" },
    { Node::reserved_continue, "Continue" },
    { Node::reserved_return, "Return" }
  };

  std::ostream& operator<<(std::ostream& os, const Node& node) {
    return os << node.to_string(0);
  }

  std::string Node::to_string(size_t n) const {
    std::string output;
    for (size_t i = 0; i < n; ++i) {
      output += "  ";
    }

    output += "[" + debug_map[type_];

    if (!token_.empty())
      output += ": " + token_;
    return output + "]\n";
  }

  Node::Node(Node::Type type, const std::string& token) : type_(type), token_(token) {
  }


  void Node::separate_declarations_and_assignments() {}
}  // namespace AST
