SYNTAX_MACRO(LeftBrace,        "{",          l_brace)
SYNTAX_MACRO(RightBrace,       "}",          r_brace)
SYNTAX_MACRO(LeftBracket,      "[",          l_bracket)
SYNTAX_MACRO(RightBracket,     "]",          r_bracket)
SYNTAX_MACRO(LeftParen,        "(",          l_paren)
SYNTAX_MACRO(RightParen,       ")",          r_paren)
SYNTAX_MACRO(Semicolon,        ";",          semicolon)
SYNTAX_MACRO(Dot,              ".",          op_b)
SYNTAX_MACRO(Enum,             "enum",       kw_block_head)
SYNTAX_MACRO(Flags,            "flags",      kw_block_head)
SYNTAX_MACRO(Struct,           "struct",     kw_struct)
SYNTAX_MACRO(Switch,           "switch",     kw_struct)
SYNTAX_MACRO(Interface,        "interface",  kw_block)
SYNTAX_MACRO(Scope,            "scope",      kw_block)
SYNTAX_MACRO(StatefulScope,    "scope!",     kw_block)
SYNTAX_MACRO(ImplicitNewline,  "\n",         newline)
SYNTAX_MACRO(ExplicitNewline,  R"(\\)",      newline)
SYNTAX_MACRO(EndOfFile,        "",           eof)
SYNTAX_MACRO(OptBlock,         "block?",     kw_block)
SYNTAX_MACRO(RepBlock,         "block~",     kw_block)
SYNTAX_MACRO(Block,            "block",      kw_block)
SYNTAX_MACRO(Hole,             "--",         expr)
