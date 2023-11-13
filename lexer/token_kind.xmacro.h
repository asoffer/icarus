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

#if not defined(IC_XMACRO_PRIMITIVE_TYPE)
#define IC_XMACRO_PRIMITIVE_TYPE(kind, symbol, spelling)                       \
  IC_XMACRO_TOKEN_KIND_TERMINAL_EXPRESSION(kind, spelling)
#endif  // not defined(IC_XMACRO_PRIMITIVE_TYPE)
#include "common/language/primitive_types.xmacro.h"

#if not defined(IC_XMACRO_TOKEN_KIND_BINARY_OPERATOR)
#define IC_XMACRO_TOKEN_KIND_BINARY_OPERATOR(kind, symbol, precedence_group)   \
  IC_XMACRO_TOKEN_KIND_OPERATOR(kind, symbol)
#endif  // not defined(IC_XMACRO_TOKEN_KIND_BINARY_OPERATOR)

#if not defined(IC_XMACRO_TOKEN_KIND_BINARY_OR_UNARY_OPERATOR)
#define IC_XMACRO_TOKEN_KIND_BINARY_OR_UNARY_OPERATOR(kind, symbol,            \
                                                      precedence_group)        \
  IC_XMACRO_TOKEN_KIND_BINARY_OPERATOR(kind, symbol, precedence_group)
#endif  // not defined(IC_XMACRO_TOKEN_KIND_BINARY_OR_UNARY_OPERATOR)

#if not defined(IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR)
#define IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(kind, symbol,                \
                                                  precedence_group)            \
  IC_XMACRO_TOKEN_KIND_BINARY_OPERATOR(kind, symbol, precedence_group)
#endif  // not defined(IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR)

#if not defined(IC_XMACRO_TOKEN_KIND_ONE_CHARACTER_TOKEN)
#define IC_XMACRO_TOKEN_KIND_ONE_CHARACTER_TOKEN(kind, symbol)                 \
  IC_XMACRO_TOKEN_KIND(kind)
#endif  // not defined(IC_XMACRO_TOKEN_KIND_ONE_CHARACTER_TOKEN)

#if not defined(IC_XMACRO_TOKEN_KIND_OPEN)
#define IC_XMACRO_TOKEN_KIND_OPEN(kind, symbol)                                \
  IC_XMACRO_TOKEN_KIND_ONE_CHARACTER_TOKEN(kind, symbol)
#endif  // not defined(IC_XMACRO_TOKEN_KIND_OPEN)

#if not defined(IC_XMACRO_TOKEN_KIND_CLOSE)
#define IC_XMACRO_TOKEN_KIND_CLOSE(kind, symbol)                               \
  IC_XMACRO_TOKEN_KIND_ONE_CHARACTER_TOKEN(kind, symbol)
#endif  // not defined(IC_XMACRO_TOKEN_KIND_CLOSE)

IC_XMACRO_TOKEN_KIND(Identifier)
IC_XMACRO_TOKEN_KIND(IntegerLiteral)
IC_XMACRO_TOKEN_KIND(StringLiteral)
IC_XMACRO_TOKEN_KIND(Newline)
IC_XMACRO_TOKEN_KIND(Eof)
IC_XMACRO_TOKEN_KIND(Invalid)

IC_XMACRO_TOKEN_KIND_KEYWORD(Let, "let")
IC_XMACRO_TOKEN_KIND_KEYWORD(Var, "var")
IC_XMACRO_TOKEN_KIND_KEYWORD(Import, "import")
IC_XMACRO_TOKEN_KIND_KEYWORD(If, "if")
IC_XMACRO_TOKEN_KIND_KEYWORD(Else, "else")
IC_XMACRO_TOKEN_KIND_KEYWORD(Fn, "fn")
IC_XMACRO_TOKEN_KIND_KEYWORD(Return, "return")

IC_XMACRO_TOKEN_KIND_TERMINAL_EXPRESSION(True, "true")
IC_XMACRO_TOKEN_KIND_TERMINAL_EXPRESSION(False, "false")
IC_XMACRO_TOKEN_KIND_TERMINAL_EXPRESSION(Builtin, "builtin")

IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(MinusGreater, "->", Function)
IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(Plus, "+", PlusMinus)
IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(Percent, "%", Modulus)
IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(Less, "<", Comparison)
IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(Greater, ">", Comparison)
IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(LessEqual, "<=", Comparison)
IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(GreaterEqual, ">=", Comparison)
IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(EqualEqual, "==", Comparison)
IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR(NotEqual, "!=", Comparison)

IC_XMACRO_TOKEN_KIND_BINARY_OR_UNARY_OPERATOR(Minus, "-", PlusMinus)

IC_XMACRO_TOKEN_KIND_OPERATOR(Backslash, "\\")
IC_XMACRO_TOKEN_KIND_OPERATOR(ColonColonEqual, "::=")
IC_XMACRO_TOKEN_KIND_OPERATOR(ColonColon, "::")
IC_XMACRO_TOKEN_KIND_OPERATOR(ColonEqual, ":=")
IC_XMACRO_TOKEN_KIND_OPERATOR(Colon, ":")
IC_XMACRO_TOKEN_KIND_OPERATOR(Star, "*")
IC_XMACRO_TOKEN_KIND_OPERATOR(BracketedStar, "[*]")
IC_XMACRO_TOKEN_KIND_OPERATOR(Equal, "=")
IC_XMACRO_TOKEN_KIND_OPERATOR(Period, ".")

IC_XMACRO_TOKEN_KIND_ONE_CHARACTER_TOKEN(Comma, ',')
IC_XMACRO_TOKEN_KIND_ONE_CHARACTER_TOKEN(Semicolon, ';')

// NOTE: The relationship between these is important. We rely on the fact that
// open/close-pairs are enumerated adjacent to one another.
IC_XMACRO_TOKEN_KIND_OPEN(LeftParen, '(')
IC_XMACRO_TOKEN_KIND_CLOSE(RightParen, ')')
IC_XMACRO_TOKEN_KIND_OPEN(LeftBrace, '{')
IC_XMACRO_TOKEN_KIND_CLOSE(RightBrace, '}')
// IC_XMACRO_TOKEN_KIND_OPEN(LeftBracket, '[')
// IC_XMACRO_TOKEN_KIND_CLOSE(RightBracket, ']')

#undef IC_XMACRO_TOKEN_KIND
#undef IC_XMACRO_TOKEN_KIND_KEYWORD
#undef IC_XMACRO_TOKEN_KIND_TERMINAL_EXPRESSION
#undef IC_XMACRO_TOKEN_KIND_OPERATOR
#undef IC_XMACRO_TOKEN_KIND_OPERATOR_WORD
#undef IC_XMACRO_TOKEN_KIND_BINARY_ONLY_OPERATOR
#undef IC_XMACRO_TOKEN_KIND_BINARY_OR_UNARY_OPERATOR
#undef IC_XMACRO_TOKEN_KIND_ONE_CHARACTER_TOKEN
#undef IC_XMACRO_TOKEN_KIND_BINARY_OPERATOR
#undef IC_XMACRO_TOKEN_KIND_OPEN
#undef IC_XMACRO_TOKEN_KIND_CLOSE
