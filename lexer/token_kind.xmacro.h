#if not defined(IC_XMACRO_TOKEN_KIND)
#define IC_XMACRO_TOKEN_KIND(kind)
#endif  // not defined(IC_XMACRO_TOKEN_KIND)

#if not defined(IC_XMACRO_TOKEN_KIND_OPERATOR)
#define IC_XMACRO_TOKEN_KIND_OPERATOR(kind, symbol) IC_XMACRO_TOKEN_KIND(kind)
#endif  // not defined(IC_XMACRO_TOKEN_KIND_OPERATOR)

#if not defined(IC_XMACRO_TOKEN_KIND_KEYWORD)
#define IC_XMACRO_TOKEN_KIND_KEYWORD(kind, keyword) IC_XMACRO_TOKEN_KIND(kind)
#endif  // not defined(IC_XMACRO_TOKEN_KIND_KEYWORD)

#if not defined(IC_XMACRO_TOKEN_KIND_TERMINAL_EXPRESSION)
#define IC_XMACRO_TOKEN_KIND_TERMINAL_EXPRESSION(kind, keyword)                \
  IC_XMACRO_TOKEN_KIND_KEYWORD(kind, keyword)
#endif  // not defined(IC_XMACRO_TOKEN_KIND_TERMINAL_EXPRESSION)

#if not defined(IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE)
#define IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE(kind, symbol, spelling)              \
  IC_XMACRO_TOKEN_KIND_TERMINAL_EXPRESSION(kind, spelling)
#endif  // not defined(IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE)

#if not defined(IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR)
#define IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(kind, symbol,                \
                                                  precedence_group)            \
  IC_XMACRO_TOKEN_KIND_OPERATOR(kind, symbol)
#endif  // not defined(IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR)

IC_XMACRO_TOKEN_KIND(Identifier)
IC_XMACRO_TOKEN_KIND(IntegerLiteral)
IC_XMACRO_TOKEN_KIND(Newline)
IC_XMACRO_TOKEN_KIND(Eof)
IC_XMACRO_TOKEN_KIND(Invalid)

IC_XMACRO_TOKEN_KIND_KEYWORD(Let, "let")
IC_XMACRO_TOKEN_KIND_KEYWORD(Var, "var")

IC_XMACRO_TOKEN_KIND_TERMINAL_EXPRESSION(True, "true")
IC_XMACRO_TOKEN_KIND_TERMINAL_EXPRESSION(False, "false")

IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE(Bool, Bool, "bool")
IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE(Char, Char, "char")
IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE(Byte, Byte, "byte")
IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE(I8, I8, "i8")
IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE(I16, I16, "i16")
IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE(I32, I32, "i32")
IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE(I64, I64, "i64")
IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE(U8, U8, "u8")
IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE(U16, U16, "u16")
IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE(U32, U32, "u32")
IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE(U64, U64, "u64")
IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE(F32, F32, "f32")
IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE(F64, F64, "u64")
IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE(Integer, Integer, "integer")
IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE(Type, Type_, "type")
IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE(Module, Module, "module")

IC_XMACRO_TOKEN_KIND_OPERATOR(ColonColonEqual, "::=")
IC_XMACRO_TOKEN_KIND_OPERATOR(ColonColon, "::")
IC_XMACRO_TOKEN_KIND_OPERATOR(ColonEqual, ":=")
IC_XMACRO_TOKEN_KIND_OPERATOR(Colon, ":")
IC_XMACRO_TOKEN_KIND_OPERATOR(Star, "*")
IC_XMACRO_TOKEN_KIND_OPERATOR(BracketedStar, "[*]")
IC_XMACRO_TOKEN_KIND_OPERATOR(Minus, "-")
IC_XMACRO_TOKEN_KIND_OPERATOR(Equal, "=")

IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(Plus, "+", PlusMinus)
IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(Slash, "/", MultiplyDivide)
IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(Percent, "%", Modulus)
IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(Less, "<", Comparison)
IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(Greater, ">", Comparison)
IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(LessEqual, "<=", Comparison)
IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(GreaterEqual, ">=", Comparison)
IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(EqualEqual, "==", Comparison)
IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(NotEqual, "!=", Comparison)

#undef IC_XMACRO_TOKEN_KIND
#undef IC_XMACRO_TOKEN_KIND_KEYWORD
#undef IC_XMACRO_TOKEN_KIND_TERMINAL_EXPRESSION
#undef IC_XMACRO_TOKEN_KIND_OPERATOR
#undef IC_XMACRO_TOKEN_KIND_BUILTIN_TYPE
#undef IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR
