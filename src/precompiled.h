#ifndef ICARUS_PRECOMPILED_H
#define ICARUS_PRECOMPILED_H

// Common standard headers
#include <iostream>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <queue>
#include <sstream>
#include <fstream>
#include <memory>

#include "constants_and_enums.h"
#include "base/types.h"

namespace AST {
struct Node;
struct Expression;
struct Identifier;
struct TokenNode;
struct Declaration;
struct Binop;
struct Statements;
struct FunctionLiteral;
struct ScopeLiteral;
struct CodeBlock;
} // namespace AST

struct Type;
struct Primitive;
struct Array;
struct Tuple;
struct Pointer;
struct Function;
struct Enum;
struct Struct;
struct ParamStruct;
struct TypeVariable;
struct RangeType;
struct SliceType;
struct Scope_Type;

struct Scope;
struct FnScope;

using NPtrVec = std::vector<AST::Node *>;

namespace IR {
struct Block;
struct LocalStack;
struct StackFrame;
struct Func;
} // namespace IR


inline size_t MoveForwardToAlignment(size_t ptr, size_t alignment) {
  return ((ptr - 1) | (alignment - 1)) + 1;
}

#endif // ICARUS_PRECOMPILED_H
