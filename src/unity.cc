#define ICARUS_UNITY
#include "Type.h"
#include "Scope.h"
#include "Context.h"

#include "AST/assign_scope.cpp"
#include "AST/build.cpp"
#include "AST/clone.cpp"
#include "AST/ctors.cpp"
#include "AST/dtors.cpp"
#include "AST/evaluate.cpp"
#include "AST/generate_code.cpp"
#include "AST/generate_lvalue.cpp"
#include "AST/lrval.cpp"
#include "AST/time.cpp"
#include "AST/to_string.cpp"
#include "AST/verify_types.cpp"

#include "Context.cpp"
#include "ErrorLog.cpp"
#include "externs.cpp"
#include "Lexer.cpp"
#include "Mangler.cpp"
#include "Rule.cpp"
#include "Parser.cpp"
#include "main.cpp"
#include "Scope.cpp"
#include "Type.cpp"
#include "TypeExterns.cpp"

#include "Type/allocate.cpp"
#include "Type/assign.cpp"
#include "Type/generate_llvm.cpp"
#include "Type/initialize.cpp"
#include "Type/repr.cpp"
#include "Type/to_string.cpp"
#include "Type/type_time.cpp"
#include "Type/uninitialize.cpp"

#undef ICARUS_UNITY
