#ifndef ICARUS_RULE_H
#define ICARUS_RULE_H

#include <vector>
#include <memory>
#include "AST/Node.h"

class Rule {
  public:
    typedef std::unique_ptr<AST::Node> NPtr;

    Rule(AST::Node::Type output, const std::vector<AST::Node::Type>& input, int prec);

    inline int precedence() const { return precedence_; }

    bool match(const std::vector<NPtr>& node_stack) const;
    void apply(std::vector<NPtr>& node_stack) const;

  private:
    AST::Node::Type output_;
    std::vector<AST::Node::Type> input_;
    int precedence_;
};

#endif  // ICARUS_RULE_H
