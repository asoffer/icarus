#ifndef ICARUS_AST_SCOPE_SCOPE_LIT_H
#define ICARUS_AST_SCOPE_SCOPE_LIT_H

namespace ast {

// Represents the body of a scope-literal which is the construct responsible for
// defining scopes.
struct ScopeLitScope : public DeclScope {
  ScopeLitScope(Scope *parent) : DeclScope(parent) {}
};

}  // namespace ast

#endif  // ICARUS_AST_SCOPE_SCOPE_LIT_H
