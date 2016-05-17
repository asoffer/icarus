#ifndef ICARUS_AST_H
#define ICARUS_AST_H

namespace Language {
extern size_t precedence(Language::Operator op);
} // namespace Language

extern std::queue<std::string> file_queue;

namespace AST {
#define ENDING = 0
#define OVERRIDE
#define VIRTUAL_METHODS_FOR_NODES                                              \
  virtual std::string to_string(size_t n) const ENDING;                        \
  virtual void join_identifiers(bool is_arg = false) ENDING;                   \
  virtual void assign_scope() ENDING;                                          \
  virtual void lrvalue_check() ENDING;                                         \
  virtual void verify_types() ENDING;                                          \
  virtual std::string graphviz_label() const ENDING;                           \
  virtual Context::Value evaluate(Context &ctx) ENDING;                        \
  virtual llvm::Value *generate_code() ENDING;                                 \
  virtual Time::Eval determine_time() ENDING;                                  \
  virtual Node *clone(size_t num_entries, TypeVariable **lookup_key,           \
                      Type **lookup_val) ENDING

#define EXPR_FNS(name, checkname)                                              \
  name();                                                                      \
  virtual ~name();                                                             \
  virtual bool is_##checkname() const OVERRIDE { return true; }                \
  virtual std::string to_string(size_t n) const ENDING;                        \
  virtual void join_identifiers(bool is_arg = false) ENDING;                   \
  virtual void lrvalue_check() ENDING;                                         \
  virtual void assign_scope() ENDING;                                          \
  virtual void verify_types() ENDING;                                          \
  virtual std::string graphviz_label() const ENDING;                           \
  virtual llvm::Value *generate_code() ENDING;                                 \
  virtual llvm::Value *generate_lvalue() ENDING;                               \
  virtual Context::Value evaluate(Context &ctx) ENDING;                        \
  virtual Time::Eval determine_time() ENDING;                                  \
  virtual llvm::Constant *GetGlobal(Context &ctx) ENDING;                      \
  virtual Node *clone(size_t num_entries, TypeVariable **lookup_key,           \
                      Type **lookup_val) ENDING

struct Node {
  virtual std::string token() const { return token_; }

  virtual std::string to_string(size_t n) const = 0;
  virtual void join_identifiers(bool = false) {}
  virtual void lrvalue_check() {}
  virtual void assign_scope() {}
  virtual void verify_types() {}
  virtual std::string graphviz_label() const;
  virtual Node *clone(size_t num_entries, TypeVariable **lookup_key,
                      Type **lookup_val);

  virtual Context::Value evaluate(Context &) { return nullptr; }
  virtual llvm::Value *generate_code() { return nullptr; }
  virtual Time::Eval determine_time() { return Time::error; }

  Time::Eval time() { return time_; }

  virtual bool is_identifier() const { return false; }
  virtual bool is_terminal() const { return false; }
  virtual bool is_expression() const { return false; }
  virtual bool is_binop() const { return false; }
  virtual bool is_function_literal() const { return false; }
  virtual bool is_chain_op() const { return false; }
  virtual bool is_case() const { return false; }
  virtual bool is_unop() const { return false; }
  virtual bool is_access() const { return false; }
  virtual bool is_comma_list() const { return false; }
  virtual bool is_in_decl() const { return false; }
  virtual bool is_declaration() const { return false; }
  virtual bool is_array_type() const { return false; }
  virtual bool is_parametric_struct_literal() const { return false; }
  virtual bool is_struct_literal() const { return false; }
  virtual bool is_statements() const { return false; }
  virtual bool is_enum_literal() const { return false; }
  virtual bool is_array_literal() const { return false; }
  virtual bool is_token_node() const { return false; }
  virtual bool is_dummy() const { return false; }
  virtual bool is_jump() const { return false; }

  //TODO there's a cheaper way to do this (without string comparisons).
  bool is_hole() const { return token() == "--"; }

  Node(TokenLocation loc = TokenLocation(), const std::string &token = "")
      : scope_(nullptr), token_(token), loc(loc), time_(Time::error) {}

  virtual ~Node() {}

  inline friend std::ostream &operator<<(std::ostream &os, const Node &node) {
    return os << node.to_string(0);
  }

  Scope *scope_;

  std::string token_;
  TokenLocation loc;
  Time::Eval time_;
};

struct Expression : public Node {
  EXPR_FNS(Expression, expression);
  static Node *build(NPtrVec &&nodes);

  llvm::Value *llvm_value(Context::Value v);

  size_t precedence;
  bool lvalue;
  Type *type;
  Context::Value value;
};

struct TokenNode : public Node {
  virtual std::string to_string(size_t n) const;

  virtual Node *clone(size_t num_entries, TypeVariable **lookup_key,
                      Type **lookup_val);

  virtual bool is_token_node() const { return true; }
  virtual std::string token() const { return tk_; }

  virtual ~TokenNode() {}

  // TODO make newline default a bof (beginning of file)
  TokenNode(TokenLocation loc = TokenLocation(), std::string str_lit = "");

  std::string tk_;
  Language::Operator op;
};

#undef ENDING
#define ENDING override
#undef OVERRIDE
#define OVERRIDE override

struct Terminal : public Expression {
  EXPR_FNS(Terminal, terminal);
  Language::Terminal terminal_type;
};

struct Identifier : public Terminal {
  EXPR_FNS(Identifier, identifier);
  Identifier(TokenLocation loc, const std::string &token_string);

  void AppendType(Type *t);

  llvm::Value *alloc;
  std::vector<Declaration *> decls; // multiple because function overloading
  bool is_arg;                      // function argument or struct parameter
};

struct Binop : public Expression {
  EXPR_FNS(Binop, binop);
  static Node *BuildElseRocket(NPtrVec &&nodes);
  static Node *BuildCallOperator(NPtrVec &&nodes);
  static Node *BuildIndexOperator(NPtrVec &&nodes);
  static Node *BuildArrayType(NPtrVec &&nodes);

  bool is_assignment() const {
    using Language::Operator;
    return op == Operator::Assign || op == Operator::OrEq ||
           op == Operator::XorEq || op == Operator::AndEq ||
           op == Operator::AddEq || op == Operator::SubEq ||
           op == Operator::MulEq || op == Operator::DivEq ||
           op == Operator::ModEq;
  }

  Language::Operator op;
  Expression *lhs, *rhs;
};

struct Declaration : public Expression {
  EXPR_FNS(Declaration, declaration);
  static Node *BuildBasic(NPtrVec &&nodes);
  static Node *BuildGenerate(NPtrVec &&nodes);

  static Node *AddHashtag(NPtrVec &&nodes);

  // TODO is 'op' necessary? anymore?
  Language::Operator op;
  Identifier *identifier;
  Expression *expr;

  std::vector<std::string> hashtags;
  // TODO have a global table of hashtags and store a vector of indexes into
  // that global lookup.
  bool HasHashtag(const std::string &str) const {
    for (const auto& tag : hashtags) {
      if (str == tag) return true;
    }
    return false;
  }

  DeclType decl_type;
};

struct InDecl : public Expression {
  EXPR_FNS(InDecl, in_decl);
  static Node *Build(NPtrVec &&nodes);

  Identifier *identifier;
  Expression *container;
};

struct ParametricStructLiteral : public Expression {
  EXPR_FNS(ParametricStructLiteral, parametric_struct_literal);
  static Node *Build(NPtrVec &&nodes);

  StructLiteral *CloneStructLiteral(StructLiteral *&, Context &ctx);

  std::vector<Declaration *> params;
  std::vector<llvm::Constant *> init_vals;

  Scope *type_scope;
  std::vector<Declaration *> declarations;

  // TODO this should be more than just type pointers. Parameters can be ints,
  // etc. Do we allow real? Make this hold a vector of Context::Values
  std::map<std::vector<Type *>, StructLiteral *> cache;
};

struct StructLiteral : public Expression {
  EXPR_FNS(StructLiteral, struct_literal);
  static Node *Build(NPtrVec &&nodes);

  void FlushOut();

  std::vector<llvm::Constant *> init_vals;

  Scope *type_scope;
  std::vector<Declaration *> declarations;
};

struct Statements : public Node {
  static Node *build_one(NPtrVec &&nodes);
  static Node *build_more(NPtrVec &&nodes);

  VIRTUAL_METHODS_FOR_NODES;

  inline size_t size() { return statements.size(); }
  inline void reserve(size_t n) { return statements.reserve(n); }

  bool is_statements() const override { return true; }

  void add_nodes(Statements *stmts) {
    for (auto &stmt : stmts->statements) {
      statements.push_back(std::move(stmt));
    }
  }

  Statements() {}
  virtual ~Statements();

  std::vector<AST::Node *> statements;
};

struct Unop : public Expression {
  EXPR_FNS(Unop, unop);
  static Node *BuildLeft(NPtrVec &&nodes);
  static Node *BuildParen(NPtrVec &&nodes);
  static Node *BuildDots(NPtrVec &&nodes);

  Expression *operand;
  Language::Operator op;
};

struct Access : public Expression {
  EXPR_FNS(Access, access);
  static Node *Build(NPtrVec &&nodes);

  std::string member_name;
  Expression *operand;
};

struct ChainOp : public Expression {
  EXPR_FNS(ChainOp, chain_op);
  static Node *Build(NPtrVec &&nodes);

  static Node *join(NPtrVec &&nodes);

  virtual bool is_comma_list() const override {
    return ops.front() == Language::Operator::Comma;
  }

  std::vector<Language::Operator> ops;
  std::vector<Expression *> exprs;
};

struct ArrayLiteral : public Expression {
  EXPR_FNS(ArrayLiteral, array_literal);
  static Node *build(NPtrVec &&nodes);
  static Node *BuildEmpty(NPtrVec &&nodes);

  std::vector<Expression *> elems;
};

struct ArrayType : public Expression {
  EXPR_FNS(ArrayType, array_type);
  static Node *build(NPtrVec &&nodes);

  Expression *length;
  Expression *data_type;
};

struct Case : public Expression {
  EXPR_FNS(Case, case);
  static Node *Build(NPtrVec &&nodes);

  std::vector<std::pair<Expression *, Expression *>> key_vals;
};

struct FunctionLiteral : public Expression {
  EXPR_FNS(FunctionLiteral, function_literal);
  static Node *build(NPtrVec &&nodes);

  FnScope *fn_scope;
  Expression *return_type_expr;

  std::vector<Declaration *> inputs;
  llvm::Function *llvm_fn;
  Statements *statements;

  bool code_gened;

  std::map<Type *, FunctionLiteral *> cache;
};

struct Conditional : public Node {
  static Node *BuildIf(NPtrVec &&nodes);
  static Node *build_else_if(NPtrVec &&nodes);
  static Node *build_else(NPtrVec &&nodes);

  VIRTUAL_METHODS_FOR_NODES;

  bool has_else() const { return else_line_num != 0; }

  Conditional() : else_line_num(0) {}
  virtual ~Conditional();

  std::vector<Expression *> conditions;
  std::vector<Statements *> statements;
  std::vector<BlockScope *> body_scopes;

  // We use else_line_num to determine if an else branch exists (when it's
  // non-zero) and also for error generation (if multiple else-blocks are
  // present).
  size_t else_line_num;
};

struct For : public Node {
  For();
  virtual ~For();
  VIRTUAL_METHODS_FOR_NODES;

  static Node *Build(NPtrVec &&nodes);

  std::vector<InDecl *> iterators;
  Statements *statements;

  BlockScope *for_scope;
};

struct While : public Node {
  While();
  virtual ~While();
  VIRTUAL_METHODS_FOR_NODES;

  static Node *Build(NPtrVec &&nodes);

  Expression *condition;
  Statements *statements;
  BlockScope *while_scope;
};

struct EnumLiteral : public Expression {
  EXPR_FNS(EnumLiteral, enum_literal);
  static Node *Build(NPtrVec &&nodes);

  std::vector<std::string> members;
};

struct DummyTypeExpr : public Expression {
  EXPR_FNS(DummyTypeExpr, dummy);
  static Node *build(NPtrVec &&nodes);

  DummyTypeExpr(TokenLocation loc, Type *t);
};

struct Jump : public Node {
  enum class JumpType { Restart, Continue, Repeat, Break, Return };
  virtual bool is_jump() const override { return true; }

  static Node *build(NPtrVec &&nodes);
  virtual ~Jump();

  VIRTUAL_METHODS_FOR_NODES;

  Jump(TokenLocation new_loc, JumpType jump_type);

  BlockScope *scope;
  JumpType jump_type;
};
} // namespace AST

#undef VIRTUAL_METHODS_FOR_NODES
#undef EXPR_FNS
#undef ENDING
#undef OVERRIDE

#endif // ICARUS_AST_NODE_H
