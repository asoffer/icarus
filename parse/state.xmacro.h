#if not defined(IC_XMACRO_PARSER_STATE)
#error `IC_XMACRO_PARSER_STATE` must be defined.
#endif

IC_XMACRO_PARSER_STATE(Newlines)

IC_XMACRO_PARSER_STATE(Module)

IC_XMACRO_PARSER_STATE(Statement)
IC_XMACRO_PARSER_STATE(ResolveStatement)
IC_XMACRO_PARSER_STATE(StatementSequence)
IC_XMACRO_PARSER_STATE(BracedStatementSequence)
IC_XMACRO_PARSER_STATE(ResolveStatementSequence)
IC_XMACRO_PARSER_STATE(SubsequentStatementSequence)

IC_XMACRO_PARSER_STATE(InvocationArgumentSequence)
IC_XMACRO_PARSER_STATE(ResolveInvocationArgumentSequence)

IC_XMACRO_PARSER_STATE(Declaration)
IC_XMACRO_PARSER_STATE(ColonToEndOfDeclaration)
IC_XMACRO_PARSER_STATE(DeclaredSymbol)
IC_XMACRO_PARSER_STATE(ResolveUninferredTypeDeclaration)
IC_XMACRO_PARSER_STATE(ResolveDeclaration)

IC_XMACRO_PARSER_STATE(Expression)
IC_XMACRO_PARSER_STATE(ExpressionSuffix)
IC_XMACRO_PARSER_STATE(ParenthesizedExpression)
IC_XMACRO_PARSER_STATE(ResolveMemberTerm)
IC_XMACRO_PARSER_STATE(AtomicTerm)
IC_XMACRO_PARSER_STATE(CommaSeparatedExpressionSequence)
IC_XMACRO_PARSER_STATE(CommaSeparatedDeclarationSequence)
IC_XMACRO_PARSER_STATE(ResolveFunctionTypeParameters)

IC_XMACRO_PARSER_STATE(ResolveAssignment)

IC_XMACRO_PARSER_STATE(FunctionLiteralReturnTypeStart)
IC_XMACRO_PARSER_STATE(FunctionLiteralBody)
IC_XMACRO_PARSER_STATE(ResolveFunctionLiteral)

IC_XMACRO_PARSER_STATE(IfStatementTrueBranchStart)
IC_XMACRO_PARSER_STATE(IfStatementFalseBranchStart)
IC_XMACRO_PARSER_STATE(ResolveIfStatement)
IC_XMACRO_PARSER_STATE(IfStatementTryElse)

IC_XMACRO_PARSER_STATE(ResolveSliceType)
IC_XMACRO_PARSER_STATE(ResolvePointerType)
IC_XMACRO_PARSER_STATE(ResolveBufferPointerType)
IC_XMACRO_PARSER_STATE(ResolveImport)
IC_XMACRO_PARSER_STATE(ResolveReturn)

IC_XMACRO_PARSER_STATE(ResolveExpressionGroup)

IC_XMACRO_PARSER_STATE(ClosingParenthesis)
IC_XMACRO_PARSER_STATE(ClosingBrace)

#undef IC_XMACRO_PARSER_STATE
#undef IC_XMACRO_PARSER_STATE_SEQUENCE
