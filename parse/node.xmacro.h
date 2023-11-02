// Defines an X-macro enabling iteration over all kinds of ParseNodes.
//
// Parse nodes are categorized hierarchically with `IC_XMACRO_PARSE_NODE` at the
// top (most general) of the hierarchy. The hierarchy is structured as follows,
// where each category is also categorized as the node above it.
//
// IC_XMACRO_PARSE_NODE
//  |- IC_XMACRO_PARSE_NODE_STATEMENT
//      |- IC_XMACRO_PARSE_NODE_DECLARATION
//      |- IC_XMACRO_PARSE_NODE_EXPRESSION
//          |- IC_XMACRO_PARSE_NODE_CONSTANT
//
// Each of these macros is separately definable. Any left undefined will expand
// to nothing. This means that one could extract all constants simply by
// defining `IC_XMACRO_PARSE_NODE_CONSTANT`. Or one could define
// `IC_XMACRO_PARSE_NODE` to obtain all parse nodes. One could also define both
// to get all parse nodes but have specialized treatment for constant parse
// nodes.
//
#if not defined(IC_XMACRO_PARSE_NODE)
#define IC_XMACRO_PARSE_NODE(name)
#endif  // not defined(IC_XMACRO_PARSE_NODE)

#if not defined(IC_XMACRO_PARSE_NODE_STATEMENT)
#define IC_XMACRO_PARSE_NODE_STATEMENT(name) IC_XMACRO_PARSE_NODE(name)
#endif  // not defined(IC_XMACRO_PARSE_NODE_STATEMENT)

#if not defined(IC_XMACRO_PARSE_NODE_DECLARATION)
#define IC_XMACRO_PARSE_NODE_DECLARATION(name) IC_XMACRO_PARSE_NODE(name)
#endif  // not defined(IC_XMACRO_PARSE_NODE_DECLARATION)

#if not defined(IC_XMACRO_PARSE_NODE_EXPRESSION)
#define IC_XMACRO_PARSE_NODE_EXPRESSION(name) IC_XMACRO_PARSE_NODE(name)
#endif  // not defined(IC_XMACRO_PARSE_NODE_EXPRESSION)

#if not defined(IC_XMACRO_PARSE_NODE_CONSTANT)
#define IC_XMACRO_PARSE_NODE_CONSTANT(name, type)                              \
  IC_XMACRO_PARSE_NODE_EXPRESSION(name)
#endif  // not defined(IC_XMACRO_PARSE_NODE_CONSTANT)

IC_XMACRO_PARSE_NODE(StatementSequence)
IC_XMACRO_PARSE_NODE(Let)
IC_XMACRO_PARSE_NODE(Var)
IC_XMACRO_PARSE_NODE(DeclaredIdentifier)
IC_XMACRO_PARSE_NODE(ColonColonEqual)
IC_XMACRO_PARSE_NODE(ColonEqual)
IC_XMACRO_PARSE_NODE(ColonColon)
IC_XMACRO_PARSE_NODE(Colon)
IC_XMACRO_PARSE_NODE(ScopeStart)
IC_XMACRO_PARSE_NODE(FunctionStart)
IC_XMACRO_PARSE_NODE(BeginIfStatementTrueBranch)
IC_XMACRO_PARSE_NODE(InfixOperator)
IC_XMACRO_PARSE_NODE(InvocationArgumentStart)
IC_XMACRO_PARSE_NODE(FunctionTypeParameters)
IC_XMACRO_PARSE_NODE_DECLARATION(Declaration)
IC_XMACRO_PARSE_NODE_STATEMENT(Statement)
IC_XMACRO_PARSE_NODE_STATEMENT(IfStatement)
IC_XMACRO_PARSE_NODE_EXPRESSION(Identifier)
IC_XMACRO_PARSE_NODE_EXPRESSION(ExpressionPrecedenceGroup)
IC_XMACRO_PARSE_NODE_EXPRESSION(ExpressionGroup)
IC_XMACRO_PARSE_NODE_EXPRESSION(MemberExpression)
IC_XMACRO_PARSE_NODE_EXPRESSION(Import)
IC_XMACRO_PARSE_NODE_EXPRESSION(Pointer)
IC_XMACRO_PARSE_NODE_EXPRESSION(BufferPointer)
IC_XMACRO_PARSE_NODE_EXPRESSION(CallExpression)
IC_XMACRO_PARSE_NODE_CONSTANT(BooleanLiteral, type::Bool)
IC_XMACRO_PARSE_NODE_CONSTANT(IntegerLiteral, type::Integer)
IC_XMACRO_PARSE_NODE_CONSTANT(StringLiteral, type::Slice(type::Char))
IC_XMACRO_PARSE_NODE_CONSTANT(TypeLiteral, type::Type_)
IC_XMACRO_PARSE_NODE_CONSTANT(BuiltinLiteral, type::Module)

#undef IC_XMACRO_PARSE_NODE
#undef IC_XMACRO_PARSE_NODE_DECLARATION
#undef IC_XMACRO_PARSE_NODE_STATEMENT
#undef IC_XMACRO_PARSE_NODE_EXPRESSION
#undef IC_XMACRO_PARSE_NODE_CONSTANT
