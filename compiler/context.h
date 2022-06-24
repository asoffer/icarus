#ifndef ICARUS_COMPILER_CONTEXT_H
#define ICARUS_COMPILER_CONTEXT_H

#include <forward_list>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/node_hash_map.h"
#include "absl/container/node_hash_set.h"
#include "absl/hash/hash.h"
#include "ast/ast.h"
#include "base/guarded.h"
#include "compiler/bound_parameters.h"
#include "compiler/call_metadata.h"
#include "ir/module.h"
#include "ir/subroutine.h"
#include "ir/value/reg.h"
#include "ir/value/scope.h"
#include "module/module.h"
#include "type/qual_type.h"

namespace compiler {
namespace internal_context {

template <typename T>
using DefiningAstNodeType = std::conditional_t<
    (base::meta<T> == base::meta<type::Struct>), ast::StructLiteral,
    std::conditional_t<(base::meta<T> == base::meta<type::Enum> or
                        base::meta<T> == base::meta<type::Flags>),
                       ast::EnumLiteral, void>>;

}  // namespace internal_context

struct CompiledModule;

// Context holds all data that the compiler computes about the program by
// traversing the syntax tree. This includes type information, compiled
// functions and jumps, etc. Note that this data may be dependent on constant
// parameters to a function, jump, or struct. To account for such dependencies,
// Context is intrusively a tree. Each Context has a pointer to it's parent
// (except the root whose parent-pointer is null), as well as a map keyed on
// arguments whose values hold child Context.
//
// For instance, the program
// ```
// f ::= (n :: i64) -> () {
//   size ::= n * n
//   array: [size; bool]
//   ...
// }
//
// f(1)
// f(2)
// ```
//
// would have three Context nodes. The root node, which has the other two nodes
// as children. These nodes are keyed on the arguments to `f`, one where `n` is
// 1 and one where `n` is 2. Note that the type of `array` is not available at
// the root node as it's type is dependent on `n`. Rather, on the two child
// nodes it has type `[1; bool]` and `[4; bool]` respectively.  Moreover, even
// the type of `size` (despite always being `i64` is not available on the root
// node. Instead, it is available on all child nodes with the same value of
// `i64`.
//
// Though there is nothing special about recursive instantiations, it's worth
// describing an example as well:
//
// ```
// pow2 ::= (n :: i64) -> i64 {
//   if (n == 0) then {
//     return 1
//   } else {
//     return pow2(n - 1) * 2
//   }
// }
//
// pow2(3)
// ```
//
// In this example, the expression `pow2(3)` instantiates a subcontext of the
// root binding, 3 to `n`. In doing so, it requires instantiating `pow2(2)`
// which becomes another subcontext of the root. This continues on so that the
// end result is that the root context has 4 subcontexts, one binding `n` to
// each of 0, 1, 2, and 3.
//
// The important thing to note here is that the subcontexts are all of the root,
// rather than in a chain. This is because we instantiate subcontexts in the
// context of the callee, not the call-site. In this case, despite there being
// two different call-sites, there is exactly one callee (namely, `pow2`) and it
// lives in the root context.
struct Context {
  Context(ir::Module *ir_mod);
  Context(Context const &) = delete;

  // Even though these special members are defaulted, they need to be defined
  // externally because otherwise we would generate the corresponding special
  // members for the incomplete type `Subcontext` below.
  Context(Context &&);
  ~Context();

  std::string DebugString() const;

  Context &root() & { return tree_.parent ? tree_.parent->root() : *this; }
  Context const &root() const & {
    return tree_.parent ? tree_.parent->root() : *this;
  }
  bool is_root() const { return tree_.parent == nullptr; }

  // Returns a Context object which has `this` as it's parent, but for which
  // `this` is not aware of the returned subcontext. This allows us to use the
  // return object as a scratchpad for computations before we know whether or
  // not we want to keep such computations. The canonical example of this is
  // when handling compile-time parameters to generic functions or structs. We
  // need to compute all parameters and arguments, but may want to throw away
  // that context if either (a) an instantiation already exists, or (b) there
  // was a substitution failure.
  //
  // TODO: Scratpads can still mutate actual output, so we should actually be
  // stashing their IR emissions elsewhere and merging if they turn out to be
  // okay.
  Context ScratchpadSubcontext();

  // InsertSubcontext:
  //
  // Returns an `InsertSubcontext`. The `inserted` bool member indicates whether
  // a dependency was inserted. In either case (inserted or already present) the
  // reference members `params` and `context` refer to the correspondingly
  // computed parameter types and `Context` into which new computed data
  // dependent on this set of generic context can be added.
  struct InsertSubcontextResult {
    core::Parameters<type::QualType> const &params;
    std::vector<type::Type> &rets;
    Context &context;
    bool inserted;
  };

  InsertSubcontextResult InsertSubcontext(
      ast::ParameterizedExpression const *node, BoundParameters const &params,
      Context &&context);

  // FindSubcontext:
  //
  // Returns a `FindSubcontextResult`. The `context` reference member refers to
  // subcontext (child subcontext, not descendant) associated with the given set
  // of parameters. This subcontext will not be created if it does not already
  // exist. It must already exist under penalty of undefined behavior.
  struct FindSubcontextResult {
    type::Function const *fn_type;
    Context &context;
  };

  FindSubcontextResult FindSubcontext(ast::ParameterizedExpression const *node,
                                      BoundParameters const &params);

  // Returns a span over a the qualified types for this expression. The span may
  // be empty if the expression's type is nothing, but behavior is undefined if
  // the expressions type is not available. The returned span is valid
  // accessible for the lifetime of this Context.
  absl::Span<type::QualType const> qual_types(
      ast::Expression const *expr) const;

  // Same as `qual_types` defined above, except that behavior is defined to
  // return a default constructed span if the expression's type is not
  // available.
  absl::Span<type::QualType const> maybe_qual_type(
      ast::Expression const *expr) const;

  // Stores the QualTypes in this context, associating them with the given
  // expression.
  absl::Span<type::QualType const> set_qual_types(
      ast::Expression const *expr, absl::Span<type::QualType const> qts);
  absl::Span<type::QualType const> set_qual_type(ast::Expression const *expr,
                                                 type::QualType const qts);

  void ForEachSubroutine(
      std::invocable<ir::Subroutine const *> auto &&f) const {
    for (auto const &compiled_fn : ir_module_.functions()) { f(&compiled_fn); }
  }

  void ForEachSubroutine(
      std::invocable<ir::Subroutine const *, module::Linkage> auto &&f) const {
    for (auto const &compiled_fn : ir_module_.functions()) {
      f(&compiled_fn, module::Linkage::Internal);
    }
  }

  std::pair<ir::LocalFnId, int> MakePlaceholder(
      ast::ParameterizedExpression const *expr);
  ir::LocalFnId Placeholder(ast::ParameterizedExpression const *expr);

  // TODO Audit everything below here
  std::pair<ir::Scope, bool> add_scope(
      ast::ParameterizedExpression const *expr) {
    type::Scope const *scope_type =
        &qual_types(expr)[0].type().as<type::Scope>();

    auto [iter, inserted] = ir_scopes_.try_emplace(expr);
    auto &entry           = iter->second;

    if (inserted) { entry = ir_module_.InsertScope(scope_type); }
    return std::pair(entry, inserted);
  }

  ir::Scope FindScope(ast::ParameterizedExpression const *expr) {
    auto iter = ir_scopes_.find(expr);
    if (iter != ir_scopes_.end()) { return iter->second; }
    return ASSERT_NOT_NULL(parent())->FindScope(expr);
  }

  ir::ScopeContext set_scope_context(
      ast::ScopeNode const *node,
      std::vector<ir::ScopeContext::block_type> &&names) {
    auto [data_iter, data_inserted] =
        scope_context_data_.emplace(std::move(names));
    SetConstant(node, ir::ScopeContext(&*data_iter));
    return ir::ScopeContext(&*data_iter);
  }

  ir::ScopeContext set_scope_context(
      ast::ScopeNode const *node,
      absl::Span<ir::ScopeContext::block_type const> names) {
    auto [data_iter, data_inserted] =
        scope_context_data_.emplace(names.begin(), names.end());
    SetConstant(node, ir::ScopeContext(&*data_iter));
    return ir::ScopeContext(&*data_iter);
  }

  void CompleteType(ast::Expression const *expr, bool success);

  type::Type arg_type(std::string_view name) const {
    auto iter = arg_type_.find(name);
    return iter == arg_type_.end() ? nullptr : iter->second;
  }

  void set_arg_type(std::string_view name, type::Type t) {
    arg_type_.emplace(name, t);
  }

  absl::Span<ast::Declaration::Id const *const> decls(
      ast::Identifier const *id) const;
  void set_decls(ast::Identifier const *id,
                 std::vector<ast::Declaration::Id const *> decls);

  template <typename T>
  internal_context::DefiningAstNodeType<T> const *AstLiteral(T const *p) const {
    // TODO: Store a bidirectional map. This could be made way more efficient.
    type::Type erased_type = p;
    for (auto const &[expr, t] : types_) {
      if (t == erased_type) {
        return &expr->template as<internal_context::DefiningAstNodeType<T>>();
      }
    }
    if (auto *parent_context = parent()) {
      return parent_context->AstLiteral(p);
    }
    return nullptr;
  }

  template <typename T, typename... Args>
  std::pair<type::Type, bool> EmplaceType(ast::Expression const *expr,
                                          Args &&... args) {
    if (type::Type *t = TryLoadType(expr)) { return std::pair(*t, false); }
    auto [iter, inserted] =
        types_.emplace(expr, type::Allocate<T>(std::forward<Args>(args)...));
    return std::pair(iter->second, true);
  }

  type::Type LoadType(ast::Expression const *expr) {
    return *ASSERT_NOT_NULL(TryLoadType(expr));
  }

  ir::CompleteResultBuffer const &SetConstant(
      ast::Declaration::Id const *id, ir::CompleteResultRef const &buffer);
  ir::CompleteResultBuffer const &SetConstant(
      ast::Declaration::Id const *id, ir::CompleteResultBuffer const &buffer);

  template <typename T>
  void SetConstant(ast::Expression const *expr, T const &value) {
    auto [iter, inserted] = constants_.try_emplace(expr);
    ASSERT(inserted == true);
    iter->second.append(value);
    if (auto const *id = expr->if_as<ast::Declaration::Id>()) {
      value_callback()(id, iter->second);
    }
  }

  ir::CompleteResultBuffer const *Constant(
      ast::Declaration::Id const *id) const;

  void LoadConstantAddress(ast::Expression const *expr,
                           ir::PartialResultBuffer &out) const;
  void LoadConstant(ast::Expression const *expr,
                    ir::CompleteResultBuffer &out) const;
  void LoadConstant(ast::Expression const *expr,
                    ir::PartialResultBuffer &out) const;
  template <typename T>
  T LoadConstant(ast::Expression const *expr) const {
    ir::CompleteResultBuffer buffer;
    LoadConstant(expr, buffer);
    return buffer[0].get<T>();
  }

  bool TryLoadConstant(ast::Declaration::Id const *id,
                       ir::PartialResultBuffer &out) const;

  CallMetadata const &CallMetadata(ast::Expression const *expr) const {
    auto iter = call_metadata_.find(expr);
    if (iter != call_metadata_.end()) { return iter->second; }
    return ASSERT_NOT_NULL(parent())->CallMetadata(expr);
  }

  void SetCallMetadata(ast::Expression const *expr, struct CallMetadata m) {
    call_metadata_.insert_or_assign(expr, std::move(m));
  }

  ir::Module &ir() { return ir_module_; }

  bool ClaimVerifyBodyTask(ast::FunctionLiteral const *node) {
    return body_is_verified_.insert(node).second;
  }

  type::Typed<ast::Expression const *> typed(
      ast::Expression const *expr) const {
    auto qts = qual_types(expr);
    ASSERT(qts.size() == 1u);
    return type::Typed<ast::Expression const *>(expr, qts[0].type());
  }

  // TODO: This is a temporary mechanism to get Id information into
  // CompiledModule as we migrate to merging these two and separating out
  // pre-compiled modules to not be AST-dependent.
  void set_qt_callback(absl::AnyInvocable<void(ast::Declaration::Id const *,
                                               type::QualType) const>
                           f) {
    ASSERT(qt_callback_ == nullptr);
    ASSERT(f != nullptr);
    qt_callback_ = std::move(f);
  }
  void set_value_callback(
      absl::AnyInvocable<void(ast::Declaration::Id const *,
                              ir::CompleteResultBuffer) const>
          f) {
    ASSERT(value_callback_ == nullptr);
    ASSERT(f != nullptr);
    value_callback_ = std::move(f);
  }

 private:
  explicit Context(Context *parent);

  absl::AnyInvocable<void(ast::Declaration::Id const *, type::QualType)
                         const> const &
  qt_callback() const {
    if (qt_callback_) { return qt_callback_; }
    return ASSERT_NOT_NULL(parent())->qt_callback();
  }

  absl::AnyInvocable<void(ast::Declaration::Id const *,
                          ir::CompleteResultBuffer) const> const &
  value_callback() const {
    if (value_callback_) { return value_callback_; }
    return ASSERT_NOT_NULL(parent())->value_callback();
  }

  // Each Context is an intrusive node in a tree structure. Each Context has a
  // pointer to it's parent (accessible via `this->parent()`, and each node owns
  // it's children.
  struct Subcontext;
  struct ContextTree {
    Context *parent = nullptr;
    absl::flat_hash_map<
        ast::ParameterizedExpression const *,
        absl::node_hash_map<BoundParameters, std::unique_ptr<Subcontext>>>
        children;
  } tree_;
  constexpr Context *parent() { return tree_.parent; }
  constexpr Context const *parent() const { return tree_.parent; }

  type::Type *TryLoadType(ast::Expression const *expr) {
    auto iter = types_.find(expr);
    if (iter != types_.end()) { return &iter->second; }
    if (parent() == nullptr) { return nullptr; }
    return parent()->TryLoadType(expr);
  }

  absl::flat_hash_set<ast::FunctionLiteral const *> body_is_verified_;

  // Types of the expressions in this context.
  absl::flat_hash_map<ast::Expression const *, std::vector<type::QualType>>
      qual_types_;

  // Stores the types of argument bound to the parameter with the given name.
  absl::flat_hash_map<std::string_view, type::Type> arg_type_;

  // A map from each identifier to all possible declaration identifiers that the
  // identifier might refer to.
  absl::flat_hash_map<ast::Identifier const *,
                      std::vector<ast::Declaration::Id const *>>
      decls_;

  // A map where we store all constants that are valuable to cache during
  // compilation.
  absl::flat_hash_map<ast::Expression const *, ir::CompleteResultBuffer>
      constants_;

  // TODO: Determine whether this needs to be node or if flat is okay.
  absl::node_hash_map<ast::Expression const *, struct CallMetadata>
      call_metadata_;

  absl::flat_hash_map<ast::ParameterizedExpression const *, ir::LocalFnId>
      placeholder_fn_;
  absl::node_hash_map<ast::ParameterizedExpression const *, ir::Scope>
      ir_scopes_;

  // Holds all information about generated IR.
  ir::Module &ir_module_;

  // For types defined by a single literal expression, (e.g., enums, flags, and
  // structs), this map encodes that definition.
  absl::flat_hash_map<ast::Expression const *, type::Type> types_;

  absl::node_hash_set<std::vector<ir::ScopeContext::block_type>>
      scope_context_data_;

  absl::AnyInvocable<void(ast::Declaration::Id const *, type::QualType) const>
      qt_callback_;
  absl::AnyInvocable<void(ast::Declaration::Id const *,
                          ir::CompleteResultBuffer) const>
      value_callback_;
};

// TODO: Probably deserves it's own translation unit?
type::Type TerminalType(ast::Terminal const &node);

}  // namespace compiler

#endif  // ICARUS_COMPILER_CONTEXT_H
