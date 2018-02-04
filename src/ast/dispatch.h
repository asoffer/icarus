#ifndef ICARUS_AST_DISPATCH_H
#define ICARUS_AST_DISPATCH_H

#include <map>
#include <string>
#include <unordered_map>
#include <optional>

#include "fn_args.h"

struct Type;

namespace AST {
struct Expression;

// Represents a particular call resolution.
struct Binding {
  static std::optional<Binding>
  MakeUntyped(Expression *fn_expr, IR::Func *fn,
              const FnArgs<base::owned_ptr<Expression>> &args,
              const std::unordered_map<std::string, size_t> &index_lookup);

  bool defaulted(size_t i) const { return exprs_[i].second == nullptr; }

  Expression *fn_expr_ = nullptr;
  IR::Func *fn_        = nullptr;
  std::vector<std::pair<Type *, Expression *>> exprs_;

private:
  Binding(Expression *fn_expr, IR::Func *fn);
};

struct DispatchTable {
  // TODO come up with a good internal representaion.
  // * Can/should this be balanced to find the right type-check sequence in a
  //   streaming manner?
  // * Add weights for PGO optimizations?

  void insert(FnArgs<Type *> call_arg_types, Binding binding);

  std::map<FnArgs<Type *>, Binding> bindings_;
  size_t total_size_ = 0;
};
} // namespace AST

#endif // ICARUS_AST_DISPATCH_H
