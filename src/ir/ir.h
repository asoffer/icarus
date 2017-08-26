#ifndef ICARUS_IR_IR_H
#define ICARUS_IR_IR_H

#include <limits>
#include <memory>
#include <queue>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "../base/debug.h"
#include "../base/types.h"
#include "../base/util.h"
#include "../base/variant.h"
#include "../cursor.h"

struct Type;
struct Function;
struct Enum;
struct Pointer;

extern Type *Err, *Unknown, *Bool, *Char, *Int, *Real, *Code, *Type_, *Uint,
    *Void, *NullPtr, *String;

namespace AST {
struct Expression;
struct CodeBlock;
struct ScopeLiteral;
} // namespace AST

namespace IR {
struct Val;
struct Func;

enum class Validity : char { Always, MaybeNot, Unknown, Never };

struct Property {
  Property(const Cursor &loc) : loc(loc) {}
  virtual ~Property() {}
  virtual Validity Validate(const Val &val) const = 0;

  Cursor loc;
};

template <typename T> struct UpperBound : public Property {
  ~UpperBound() override {}
  UpperBound(const Cursor &loc, T val = std::numeric_limits<T>::max())
      : Property(loc), max_(val) {}

  Validity Validate(const Val &val) const override;

  static UpperBound<T> Merge(const std::vector<UpperBound<T>> &props) {
    UpperBound<T> result = std::numeric_limits<T>::min();
    for (const auto &prop : props) {
      result.max_ = std::max(result.max_, prop.max_);
    }
    return result;
  }

  T max_;
};

DEFINE_STRONG_INT(Register, i32, -1);
DEFINE_STRONG_INT(Argument, i32, -1);
DEFINE_STRONG_INT(BlockIndex, i32, -1);
DEFINE_STRONG_INT(EnumVal, size_t, 0);
} // namespace IR

DEFINE_STRONG_INT_HASH(IR::Register);
DEFINE_STRONG_INT_HASH(IR::Argument);
DEFINE_STRONG_INT_HASH(IR::BlockIndex);

namespace IR {
struct Addr {
  enum class Kind : u8 { Null, Global, Stack, Heap } kind;
  union {
    u64 as_global;
    u64 as_stack;
    void *as_heap;
  };

  std::string to_string() const;
};

bool operator==(Addr lhs, Addr rhs);
inline bool operator!=(Addr lhs, Addr rhs) { return !(lhs == rhs); }

struct Val {
  ::Type *type = nullptr;
  base::variant<Argument, Register, ::IR::Addr, bool, char, double, i64, u64,
                EnumVal, ::Type *, ::IR::Func *, AST::ScopeLiteral *,
                AST::CodeBlock *, AST::Expression *, BlockIndex, std::string,
                const std::vector<std::unique_ptr<Property>> *>
      value{false};

  static Val Arg(Type *t, i32 n) { return Val(t, Argument{n}); }
  static Val Reg(Register r, ::Type *t) { return Val(t, r); }
  static Val Addr(Addr addr, ::Type *t) { return Val(t, addr); }
  static Val GlobalAddr(u64 addr, ::Type *t);
  static Val HeapAddr(void *addr, ::Type *t);
  static Val StackAddr(u64 addr, ::Type *t);
  static Val Bool(bool b) { return Val(::Bool, b); }
  static Val Char(char c) { return Val(::Char, c); }
  static Val Real(double r) { return Val(::Real, r); }
  static Val Int(i64 n) { return Val(::Int, n); }
  static Val Uint(u64 n) { return Val(::Uint, n); }
  static Val Enum(const ::Enum *enum_type, size_t integral_val);
  static Val Type(::Type *t) { return Val(::Type_, t); }
  static Val CodeBlock(AST::CodeBlock *block) { return Val(::Code, block); }
  static Val Func(::IR::Func *fn);
  static Val Block(BlockIndex bi) { return Val(nullptr, bi); }
  static Val Void() { return Val(::Void, false); }
  static Val Null(::Type *t);
  static Val StrLit(std::string str) { return Val(::String, std::move(str)); }
  static Val
  Precondition(const std::vector<std::unique_ptr<Property>> *precondition);
  static Val Ref(AST::Expression *expr);
  static Val None() { return Val(); }
  static Val Scope(AST::ScopeLiteral *scope_lit);

  std::string to_string() const;

  Val() : type(nullptr), value(false) {}

private:
  template <typename T> Val(::Type *t, T val) : type(t), value(val) {}
};

inline bool operator==(const Val &lhs, const Val &rhs) {
  return lhs.type == rhs.type && lhs.value == rhs.value;
}
inline bool operator!=(const Val &lhs, const Val &rhs) { return !(lhs == rhs); }

enum class Op : char {
  Trunc, Extend,
  Neg, // ! for bool, - for numeric types
  Add, Sub, Mul, Div, Mod, // numeric types only
  Lt, Le, Eq, Ne, Gt, Ge, // numeric types only
  And, Or, Xor, // bool only
  Print,
  Malloc, Free,
  Load, Store,
  ArrayLength, ArrayData, PtrIncr,
  Phi, Field, Call, Cast,
  Nop,
  SetReturn, Arrow, Array, Ptr,
  Alloca,
  Contextualize,
  Validate,
};

struct Block;
struct Cmd;

struct Stack {
  Stack() = delete;
  Stack(size_t cap) : capacity_(cap), stack_(malloc(capacity_)) {}
  Stack(const Stack &) = delete;
  Stack(Stack &&other) {
    free(stack_);
    stack_          = other.stack_;
    other.stack_    = nullptr;
    other.capacity_ = other.size_ = 0;
  }
  ~Stack() { free(stack_); }

  template <typename T> T Load(size_t index) {
    ASSERT_EQ(index & (alignof(T) - 1), 0); // Alignment error
    return *reinterpret_cast<T *>(this->location(index));
  }

  template <typename T> void Store(T val, size_t index) {
    *reinterpret_cast<T *>(this->location(index)) = val;
  }

  IR::Val Push(Pointer *ptr);

  size_t capacity_ = 0;
  size_t size_     = 0;
  void *stack_     = nullptr;

private:
  void *location(size_t index) {
    ASSERT_LT(index, capacity_);
    return reinterpret_cast<void *>(reinterpret_cast<char *>(stack_) + index);
  }
};

struct ExecContext {
  ExecContext();

  struct Frame {
    Frame() = delete;
    Frame(Func *fn, std::vector<Val> arguments);

    void MoveTo(BlockIndex block_index) {
      ASSERT_GE(block_index.value, 0);
      prev_    = current_;
      current_ = block_index;
    }

    Func *fn_ = nullptr;
    BlockIndex current_;
    BlockIndex prev_;

    // Indexed first by block then by instruction number
    std::vector<Val> regs_ = {};
    std::vector<Val> args_ = {};
    std::vector<Val> rets_ = {};
  };

  Block &current_block();

  std::stack<Frame> call_stack;

  BlockIndex ExecuteBlock();
  Val ExecuteCmd(const Cmd &cmd);
  void Resolve(Val *v) const;

  Val reg(Register r) const {
    ASSERT_GE(r.value, 0);
    return call_stack.top().regs_[static_cast<u32>(r.value)];
  }
  Val &reg(Register r) {
    ASSERT_GE(r.value, 0);
    return call_stack.top().regs_[static_cast<u32>(r.value)];
  }
  Val arg(Argument a) const { return call_stack.top().args_[a.value]; }

  Stack stack_;
};

struct Cmd {
  Cmd() : op_code(Op::Nop), result(IR::Val::None()) {}
  Cmd(Type *t, Op op, std::vector<Val> args);
  std::vector<Val> args;
  Op op_code;

  Val result; // Will always be of Kind::Reg.

  void dump(size_t indent) const;
};

Val Neg(Val v);
Val Trunc(Val v);
Val Extend(Val v);
Val Add(Val v1, Val v2);
Val Sub(Val v1, Val v2);
Val Mul(Val v1, Val v2);
Val Div(Val v1, Val v2);
Val Mod(Val v1, Val v2);
Val Lt(Val v1, Val v2);
Val Le(Val v1, Val v2);
Val Eq(Val v1, Val v2);
Val Ne(Val v1, Val v2);
Val Ge(Val v1, Val v2);
Val Gt(Val v1, Val v2);
Val And(Val v1, Val v2);
Val Or(Val v1, Val v2);
Val Xor(Val v1, Val v2);
Val Print(Val v);
Val Index(Val v1, Val v2);
Val Cast(Val result_type, Val val);
Val Call(Val fn, std::vector<Val> vals);
Val SetReturn(size_t n, Val v);
Val Load(Val v);
Val Store(Val val, Val loc);
Val ArrayLength(Val v);
Val ArrayData(Val v);
Val PtrIncr(Val v1, Val v2);
Val Malloc(Type *t, Val v);
Val Free(Val v);
Val Phi(Type *t);
Val Field(Val v, size_t n);
Val Arrow(Val v1, Val v2);
Val Array(Val v1, Val v2);
Val Ptr(Val v1);
Val Alloca(Type *t);
Val Contextualize(AST::CodeBlock *code, std::vector<IR::Val> args);
Val Validate(Val v1,
             const std::vector<std::unique_ptr<Property>> *precondition);

struct Jump {
  static Jump &Current();

  static void Unconditional(BlockIndex index) {
    Jump &jmp       = Current();
    jmp.block_index = index;
    jmp.type        = Type::Uncond;
  }

  static void Conditional(Val cond, BlockIndex true_index,
                          BlockIndex false_index);

  static void Return() { Current().type = Type::Ret; }

  void dump(size_t indent) const;

  Jump() {}
  ~Jump() {}
  struct CondData {
    Val cond;
    BlockIndex true_block;
    BlockIndex false_block;
  };

  enum class Type : u8 { None, Uncond, Cond, Ret } type = Type::None;

  // TODO reintroduce these as a union.
  BlockIndex block_index; // for unconditional jump
  CondData cond_data; // value and block indices to jump for conditional jump.
};

struct Block {
  static BlockIndex Current;
  Block() = delete;
  Block(Func *fn) : fn_(fn) {}

  void dump(size_t indent) const;

  Func *fn_; // Containing function
  std::vector<Cmd> cmds_;

  Jump jmp_;
};

struct Func {
  static Func *Current;
  static std::vector<std::unique_ptr<Func>> All;

  Func(::Function *fn_type = nullptr)
      : type(fn_type), blocks_(2, Block(this)) {}

  void dump() const;

  int ValidateCalls(std::queue<IR::Func *> *validation_queue);

  Block &block(BlockIndex index) { return blocks_[index.value]; }
  Cmd &Command(Register reg);
  void SetArgs(Register reg, std::vector<IR::Val> args);

  static BlockIndex AddBlock() {
    BlockIndex index;
    index.value = static_cast<decltype(index.value)>(Current->blocks_.size());
    Current->blocks_.emplace_back(Current);
    return index;
  }

  BlockIndex entry() const { return BlockIndex(0); }
  BlockIndex exit() const { return BlockIndex(1); }

  std::vector<Val> Execute(std::vector<Val> args, ExecContext *ctx,
                           bool *were_errors);

  // Is this needed? Or can it be determined from the containing FunctionLiteral
  // object?
  ::Function *type = nullptr;
  i32 num_cmds_    = 0;
  std::string name;
  std::vector<Block> blocks_;
  // TODO many of these maps could and should be vectors except they're keyed on
  // strong ints. Consider adding a strong int vector.
  std::unordered_map<Argument, std::vector<Register>> arg_references_;
  std::unordered_map<Register, std::vector<Register>> reg_references_;
  std::unordered_map<Register, std::pair<BlockIndex, int>> reg_map_;

  // TODO Probably a better container here. One that consolidates preconditions
  // (what about tracing errors?) and since we know how many arguments we'll
  // have ahead of time, probably a flat map or really just a vector.
  std::unordered_map<Argument, std::vector<std::unique_ptr<Property>>>
      preconditions_;
  int num_errors_ = -1; // -1 indicates not yet validated
};

template <typename T> Validity UpperBound<T>::Validate(const Val &val) const {
  return (val.value.template as<T>() < max_) ? Validity::Always
                                             : Validity::Never;
}

struct FuncResetter {
  FuncResetter(Func *fn) : old_fn_(Func::Current), old_block_(Block::Current) {
    Func::Current = fn;
  }
  ~FuncResetter() {
    Func::Current  = old_fn_;
    Block::Current = old_block_;
  }

  Func *old_fn_;
  BlockIndex old_block_;
  bool cond_ = true;
};

#define CURRENT_FUNC(fn)                                                       \
  for (auto resetter  = ::IR::FuncResetter(fn); resetter.cond_;                \
       resetter.cond_ = false)
} // namespace IR

namespace debug {
inline std::string to_string(const IR::Val &val) { return val.to_string(); }
} // namespace debug

#endif // ICARUS_IR_IR_H
