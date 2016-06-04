#include "Type/Type.h"
#include <cstdio>
#include "IR/Stack.h"

namespace debug {
extern bool ct_eval;
} // namespace debug

namespace IR {
void Cmd::Execute(StackFrame& frame) {
  std::vector<Value> cmd_inputs = args;

  // Load the command inputs from the registers or arguments if needed.
  for (auto &i : cmd_inputs) {
    if (i.flag == ValType::Ref) {
      i = frame.reg[i.val.as_ref];
    } else if (i.flag == ValType::Arg) {
      i = frame.args[i.val.as_arg];
    }
  }

  switch (op_code) {
  case Op::BNot: {
    frame.reg[result.val.as_ref] = Value(!cmd_inputs[0].val.as_bool);
  } break;
  case Op::INeg: {
    frame.reg[result.val.as_ref] = Value(-cmd_inputs[0].val.as_int);
  } break;
  case Op::FNeg: {
    frame.reg[result.val.as_ref] = Value(-cmd_inputs[0].val.as_real);
  } break;
  case Op::Load: {
    size_t offset = cmd_inputs[1].val.as_alloc;
    Type *t = cmd_inputs[0].val.as_type;
    assert(t->is_primitive() &&
           "Non-primitive local variables are not yet implemented");

#define DO_LOAD_IF(StoredType, stored_type)                                    \
  if (t == StoredType) {                                                       \
    frame.reg[result.val.as_ref] =                                             \
        Value(*(stored_type *)(frame.stack->allocs + offset));                 \
    break;                                                                     \
  }

    DO_LOAD_IF(Bool, bool);
    DO_LOAD_IF(Char, char);
    DO_LOAD_IF(Int, int);
    DO_LOAD_IF(Real, double);
    DO_LOAD_IF(Uint, size_t);
    DO_LOAD_IF(Type_, Type *);

#undef DO_LOAD_IF
  } break;
  case Op::Store: {
    size_t offset = cmd_inputs[2].val.as_alloc;
    Type *t = cmd_inputs[0].val.as_type;
    assert(t->is_primitive() &&
           "Non-primitive local variables are not yet implemented");

#define DO_STORE_IF(StoredType, store_type, type_name)                           \
  if (t == StoredType) {                                                         \
    store_type *ptr              = (store_type *)(frame.stack->allocs + offset); \
    *ptr                         = cmd_inputs[1].val.as_##type_name;             \
    frame.reg[result.val.as_ref] = Value(true);                                  \
    break;                                                                       \
  }

    DO_STORE_IF(Bool, bool, bool);
    DO_STORE_IF(Char, char, char);
    DO_STORE_IF(Int, int, int);
    DO_STORE_IF(Real, double, real);
    DO_STORE_IF(Uint, size_t, uint);
    DO_STORE_IF(Type_, Type *, type);

#undef DO_STORE_IF
  } break;
  case Op::Call: {
    auto iter = cmd_inputs.begin();
    auto fn = *iter;
    assert(fn.flag == ValType::F);
    ++iter;
    std::vector<Value> call_args;

    // TODO std::copy wasn't working for some reason I don't understand. Just
    // gonna do this by hand for now.
    for (; iter != cmd_inputs.end(); ++iter) { call_args.push_back(*iter); }
    frame.reg[result.val.as_ref] = IR::Call(fn.val.as_func, frame.stack, call_args);
  } break;
  case Op::GEP: {
    if (cmd_inputs[0].val.as_type->is_array()) {
      auto array_type = (Array *)(cmd_inputs[0].val.as_type);
      if (array_type->fixed_length) {
        if (cmd_inputs[1].flag == ValType::Alloc) {
          auto xx = cmd_inputs[1].val.as_alloc +
                    (size_t)(cmd_inputs[2].val.as_int) * sizeof(char *);

          // TODO what about alignment?
          // TODO longer sequences?
          xx += (size_t)(cmd_inputs[3].val.as_int) *
                array_type->data_type->bytes();
          frame.reg[result.val.as_ref] = Value::Alloc(xx);
        } else {
          NOT_YET;
        }
      } else {
        NOT_YET;
      }
    } else {
      NOT_YET;
    }
  } break;
  case Op::Phi: {
    for (size_t i = 0; i < incoming_blocks.size(); ++i) {
      if (frame.prev_block == incoming_blocks[i]) {
        frame.reg[result.val.as_ref] = cmd_inputs[i];
        goto exit_successfully;
      }
    }
    assert(false && "No selection made from phi block");
  exit_successfully:;
  } break;
  case Op::IAdd: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_int + cmd_inputs[1].val.as_int);
  } break;
  case Op::UAdd: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_uint + cmd_inputs[1].val.as_uint);
  } break;
  case Op::FAdd: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_real + cmd_inputs[1].val.as_real);
  } break;
  case Op::ISub: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_int - cmd_inputs[1].val.as_int);
  } break;
  case Op::USub: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_uint - cmd_inputs[1].val.as_uint);
  } break;
  case Op::FSub: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_real - cmd_inputs[1].val.as_real);
  } break;
  case Op::IMul: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_int * cmd_inputs[1].val.as_int);
  } break;
  case Op::UMul: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_uint * cmd_inputs[1].val.as_uint);
  } break;
  case Op::FMul: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_real * cmd_inputs[1].val.as_real);
  } break;
  case Op::IDiv: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_int / cmd_inputs[1].val.as_int);
  } break;
  case Op::UDiv: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_uint / cmd_inputs[1].val.as_uint);
  } break;
  case Op::FDiv: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_real / cmd_inputs[1].val.as_real);
  } break;
  case Op::IMod: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_int % cmd_inputs[1].val.as_int);
  } break;
  case Op::UMod: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_uint % cmd_inputs[1].val.as_uint);
  } break;
  case Op::FMod: {
    frame.reg[result.val.as_ref] =
        Value(fmod(cmd_inputs[0].val.as_real, cmd_inputs[1].val.as_real));
  } break;
  case Op::BXor: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_bool != cmd_inputs[1].val.as_bool);
  } break;
  case Op::ILT: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_int < cmd_inputs[1].val.as_int);
  } break;
  case Op::ULT: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_uint < cmd_inputs[1].val.as_uint);
  } break;
  case Op::FLT: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_real < cmd_inputs[1].val.as_real);
  } break;
  case Op::ILE: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_int <= cmd_inputs[1].val.as_int);
  } break;
  case Op::ULE: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_uint <= cmd_inputs[1].val.as_uint);
  } break;
  case Op::FLE: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_real <= cmd_inputs[1].val.as_real);
  } break;
  case Op::IEQ: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_int == cmd_inputs[1].val.as_int);
  } break;
  case Op::UEQ: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_uint == cmd_inputs[1].val.as_uint);
  } break;
  case Op::FEQ: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_char == cmd_inputs[1].val.as_char);
  } break;
  case Op::BEQ: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_bool == cmd_inputs[1].val.as_bool);
  } break;
  case Op::CEQ: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_char == cmd_inputs[1].val.as_char);
  } break;
  case Op::TEQ: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_type == cmd_inputs[1].val.as_type);
  } break;
  case Op::FnEQ: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_func == cmd_inputs[1].val.as_func);
  } break;
  case Op::INE: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_int != cmd_inputs[1].val.as_int);
  } break;
  case Op::UNE: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_uint != cmd_inputs[1].val.as_uint);
  } break;
  case Op::FNE: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_char != cmd_inputs[1].val.as_char);
  } break;
  case Op::BNE: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_bool != cmd_inputs[1].val.as_bool);
  } break;
  case Op::CNE: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_char != cmd_inputs[1].val.as_char);
  } break;
  case Op::TNE: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_type != cmd_inputs[1].val.as_type);
  } break;
  case Op::FnNE: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_func != cmd_inputs[1].val.as_func);
  } break;
  case Op::IGE: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_int >= cmd_inputs[1].val.as_int);
  } break;
  case Op::UGE: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_uint >= cmd_inputs[1].val.as_uint);
  } break;
  case Op::FGE: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_real >= cmd_inputs[1].val.as_real);
  } break;
  case Op::IGT: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_int > cmd_inputs[1].val.as_int);
  } break;
  case Op::UGT: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_uint > cmd_inputs[1].val.as_uint);
  } break;
  case Op::FGT: {
    frame.reg[result.val.as_ref] =
        Value(cmd_inputs[0].val.as_real > cmd_inputs[1].val.as_real);
  } break;
  case Op::Print: {
    if (cmd_inputs[0].flag == ValType::B) {
      std::printf(cmd_inputs[0].val.as_bool ? "true" : "false");

    } else if (cmd_inputs[0].flag == ValType::C) {
      std::printf("%c", cmd_inputs[0].val.as_char);

    } else if (cmd_inputs[0].flag == ValType::I) {
      std::printf("%d", cmd_inputs[0].val.as_int);

    } else if (cmd_inputs[0].flag == ValType::R) {
      std::printf("%lf", cmd_inputs[0].val.as_real);

    } else if (cmd_inputs[0].flag == ValType::U) {
      std::printf("%lu", cmd_inputs[0].val.as_uint);

    } else if (cmd_inputs[0].flag == ValType::T) {
      std::printf("%s", cmd_inputs[0].val.as_type->to_string().c_str());

    } else if (cmd_inputs[0].flag == ValType::F) {
      NOT_YET;
    }
    // Even though this operation is void, we pick a nice value like true to
    // return because in debug we want to be able to print something.
    // TODO make void return types not use registers.
    frame.reg[result.val.as_ref] = Value(true);
  } break;
  case Op::TC_Ptr: {
    frame.reg[result.val.as_ref] = Value(Ptr(cmd_inputs[0].val.as_type));
  } break;
  case Op::TC_Arrow: {
    frame.reg[result.val.as_ref] =
        Value(::Func(cmd_inputs[0].val.as_type, cmd_inputs[1].val.as_type));
  } break;
  case Op::TC_Arr1: {
    frame.reg[result.val.as_ref] = Value(Arr(cmd_inputs[0].val.as_type));
  } break;
  case Op::TC_Arr2: {
    frame.reg[result.val.as_ref] =
        Value(Arr(cmd_inputs[1].val.as_type, cmd_inputs[0].val.as_uint));
  } break;
  case Op::Bytes: {
    frame.reg[result.val.as_ref] = Value(cmd_inputs[0].val.as_type->bytes());
  } break;
  case Op::Alignment: {
    frame.reg[result.val.as_ref] = Value(cmd_inputs[0].val.as_type->alignment());
  } break;
  }
}

Block *Block::ExecuteJump(StackFrame &frame) {
  switch (exit.flag) {
  case Exit::Strategy::Uncond: return exit.true_block;
  case Exit::Strategy::ReturnVoid:
  case Exit::Strategy::Return: return nullptr;
  case Exit::Strategy::Cond: {
    Value v = exit.val;
    if (exit.val.flag == ValType::Ref) {
      v = frame.reg[v.val.as_ref];
    } else if (exit.val.flag == ValType::Arg) {
      v = frame.args[v.val.as_arg];
    }
    return v.val.as_bool ? exit.true_block : exit.false_block;
  }
  }
}

StackFrame::StackFrame(Func *f, LocalStack *local_stack,
                       const std::vector<Value> &args)
    : reg(f->num_cmds), stack(local_stack), args(args), func(f), alignment(16),
      size(f->frame_size), offset(0), inst_ptr(0), curr_block(f->entry()),
      prev_block(nullptr) {
  stack->AddFrame(this);
  // TODO determine frame alignment and make it correct! Currently just assuming
  // 16-bytes for safety.
}


void LocalStack::AddFrame(StackFrame *fr) {
  size_t old_use_size = used;

  used       = MoveForwardToAlignment(used, fr->alignment);
  fr->offset = used;
  used += fr->size;

  if (used <= capacity) { return; }

  while (used > capacity) { capacity *= 2; }
  char *new_allocs = (char *)malloc(capacity);
  memcpy(new_allocs, allocs, old_use_size);
  free(allocs);
  allocs = new_allocs;
}

void LocalStack::RemoveFrame(StackFrame *fr) { used = fr->offset; }

Value Call(Func *f, LocalStack *local_stack, const std::vector<Value> &arg_vals) {
  StackFrame frame(f, local_stack, arg_vals);

eval_loop_start:
  if (debug::ct_eval) {
    std::cerr << "\033[2J\033[1;1H" << std::endl;
    frame.curr_block->dump();

    std::cerr << std::hex;
    for (size_t i = 0; i < frame.stack->used; ++i) {
      int x = frame.stack->allocs[i];
      std::cerr << x << " ";
    }
    std::cerr << std::dec << std::endl;

    std::cerr << frame.func << ".block-" << frame.curr_block->block_num << ": "
              << frame.inst_ptr << std::endl;

        std::cout << frame.offset << std::endl;
    std::cin.ignore(1);
  }
  if (frame.inst_ptr == frame.curr_block->cmds.size()) {
    frame.prev_block = frame.curr_block;
    frame.curr_block = frame.curr_block->ExecuteJump(frame);

    if (!frame.curr_block) { // It's a return (perhaps void)
      if (frame.prev_block->exit.flag == Exit::Strategy::ReturnVoid) {
        return Value(nullptr);

      } else if (frame.prev_block->exit.val.flag == ValType::Ref) {
        return frame.reg[frame.prev_block->exit.val.val.as_ref];

      } else if (frame.prev_block->exit.val.flag == ValType::Arg) {
        return frame.reg[frame.prev_block->exit.val.val.as_arg];

      } else {
        return frame.prev_block->exit.val;
      }

    } else {
      frame.inst_ptr = 0;
      if (debug::ct_eval) {
        std::cerr << "\033[2J\033[1;1H" << std::endl;
        frame.curr_block->dump();

        std::cerr << std::hex;
        for (size_t i = 0; i < frame.stack->used; ++i) {
          int x = frame.stack->allocs[i];
          std::cerr << x << " ";
        }

        std::cerr << std::dec << "\n  jumped to block-"
                  << frame.curr_block->block_num << std::endl;
        std::cout << frame.offset << std::endl;
        std::cin.ignore(1);
      }
    }

  } else {
    auto cmd = frame.curr_block->cmds[frame.inst_ptr];
    assert(cmd.result.flag == ValType::Ref);
    cmd.Execute(frame);

    if (debug::ct_eval) {
      std::cerr << "\033[2J\033[1;1H" << std::endl;
      frame.curr_block->dump();

      std::cerr << std::hex;
      for (size_t i = 0; i < frame.stack->used; ++i) {
        int x = frame.stack->allocs[i];
        std::cerr << x << " ";
      }

      std::cerr << std::dec << "\n  " << cmd.result << " = "
                << frame.reg[cmd.result.val.as_ref] << std::endl;

        std::cout << frame.offset << std::endl;
      std::cin.ignore(1);
    }
    ++frame.inst_ptr;
  }
  goto eval_loop_start;
}

} // namespace IR
