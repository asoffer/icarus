#include "backend/exec.h"

#include <dlfcn.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <future>
#include <iostream>
#include <memory>
#include <thread>

#include "ast/ast.h"
#include "base/util.h"
#include "core/arch.h"
#include "error/log.h"
#include "ir/basic_block.h"
#include "ir/cmd/basic.h"
#include "ir/cmd/call.h"
#include "ir/cmd/cast.h"
#include "ir/cmd/jumps.h"
#include "ir/cmd/load.h"
#include "ir/cmd/misc.h"
#include "ir/cmd/phi.h"
#include "ir/cmd/print.h"
#include "ir/cmd/register.h"
#include "ir/cmd/return.h"
#include "ir/cmd/scope.h"
#include "ir/cmd/store.h"
#include "ir/cmd/types.h"
#include "ir/cmd/util.h"
#include "ir/compiled_fn.h"
#include "ir/read_only_data.h"
#include "type/type.h"
#include "type/util.h"

// TODO CreateStruct, CreateEnum, etc should register a deleter so if we exit
// early we don't leak them. FinalizeStruct, FinalizeEnum, etc should dergeister
// the deleter without calling it.

// TODO compile-time failure. dump the stack trace and abort for Null address
// kinds

namespace ast {
struct Expression;
}  // namespace ast

namespace ir {
namespace {

type::Function const *GetType(AnyFunc f) {
  return f.is_fn() ? f.func()->type_
                   : &f.foreign().type()->as<type::Function>();
}

template <typename CmdType, typename T>
auto BinaryApply(base::untyped_buffer::const_iterator *iter, bool reg0,
                 bool reg1, backend::ExecContext *ctx) {
  using fn_type = typename CmdType::fn_type;
  if constexpr (CmdType::template IsSupported<T>()) {
    auto lhs = reg0 ? ctx->resolve<T>(iter->read<Reg>()) : iter->read<T>();
    auto rhs = reg1 ? ctx->resolve<T>(iter->read<Reg>()) : iter->read<T>();
    DEBUG_LOG("binary")(lhs, rhs);
    return fn_type{}(lhs, rhs);
  } else {
    return T{};
  }
}

template <typename CmdType, typename T>
auto UnaryApply(base::untyped_buffer::const_iterator *iter, bool reg0,
                backend::ExecContext *ctx) {
  using fn_type = typename CmdType::fn_type;
  if constexpr (CmdType::template IsSupported<T>()) {
    auto val = reg0 ? ctx->resolve<T>(iter->read<Reg>()) : iter->read<T>();
    DEBUG_LOG("unary")(val);
    return fn_type{}(val);
  } else {
    return T{};
  }
}

}  // namespace

template <typename CmdType>
BasicBlock const *ExecuteCmd(base::untyped_buffer::const_iterator *iter,
                             std::vector<Addr> const &ret_slots,
                             backend::ExecContext *ctx) {
  if constexpr (std::is_same_v<CmdType, PrintCmd>) {
    auto ctrl = iter->read<typename CmdType::control_bits>();
    PrimitiveDispatch(ctrl.primitive_type, [&](auto tag) {
      using T = typename std::decay_t<decltype(tag)>::type;
      T val   = ctrl.reg ? ctx->resolve<T>(iter->read<Reg>()) : iter->read<T>();
      DEBUG_LOG("print")(typeid(T).name());
      if constexpr (std::is_same_v<T, bool>) {
        std::cerr << (val ? "true" : "false");
      } else if constexpr (std::is_same_v<T, uint8_t>) {
        std::cerr << static_cast<unsigned int>(val);
      } else if constexpr (std::is_same_v<T, int8_t>) {
        std::cerr << static_cast<int>(val);
      } else if constexpr (std::is_same_v<T, type::Type const *>) {
        std::cerr << val->to_string();
      } else if constexpr (std::is_same_v<T, Addr>) {
        std::cerr << val.to_string();
      } else if constexpr (std::is_same_v<T, EnumVal>) {
        std::optional<std::string_view> name =
            iter->read<type::Enum const *>()->name(val);
        std::cerr << (name.has_value() ? *name : absl::StrCat(val.value));
      } else if constexpr (std::is_same_v<T, FlagsVal>) {
        auto numeric_val = val.value;
        std::vector<std::string> vals;
        auto flags_type = iter->read<type::Flags const *>();

        while (numeric_val != 0) {
          size_t mask = (numeric_val & ((~numeric_val) + 1));
          numeric_val -= mask;

          std::optional<std::string_view> name =
              flags_type->name(ir::FlagsVal(mask));
          vals.emplace_back(name.has_value() ? std::string{*name}
                                             : std::to_string(mask));
        }

        if (vals.empty()) {
          std::cerr << "(empty)";
        } else {
          auto iter = vals.begin();
          std::cerr << *iter++;
          while (iter != vals.end()) { std::cerr << " | " << *iter++; }
        }
      } else {
        std::cerr << val;
      }
    });

  } else if constexpr (std::is_same_v<CmdType, NegCmd> or
                       std::is_same_v<CmdType, NotCmd> or
                       std::is_same_v<CmdType, PtrCmd> or
                       std::is_same_v<CmdType, BufPtrCmd>) {
    auto &frame = ctx->call_stack.top();
    auto ctrl   = iter->read<typename CmdType::control_bits>();
    PrimitiveDispatch(ctrl.primitive_type, [&](auto tag) {
      using type  = typename std::decay_t<decltype(tag)>::type;
      auto result = UnaryApply<CmdType, type>(iter, ctrl.reg0, ctx);
      frame.regs_.set(GetOffset(frame.fn_, iter->read<Reg>()), result);
    });

  } else if constexpr (std::is_same_v<CmdType, AddCmd> or
                       std::is_same_v<CmdType, SubCmd> or
                       std::is_same_v<CmdType, MulCmd> or
                       std::is_same_v<CmdType, DivCmd> or
                       std::is_same_v<CmdType, ModCmd> or
                       std::is_same_v<CmdType, LtCmd> or
                       std::is_same_v<CmdType, LeCmd> or
                       std::is_same_v<CmdType, EqCmd> or
                       std::is_same_v<CmdType, NeCmd> or
                       std::is_same_v<CmdType, GeCmd> or
                       std::is_same_v<CmdType, GtCmd>) {
    auto &frame = ctx->call_stack.top();
    auto ctrl   = iter->read<typename CmdType::control_bits>();
    PrimitiveDispatch(ctrl.primitive_type, [&](auto tag) {
      using type  = typename std::decay_t<decltype(tag)>::type;
      auto result = BinaryApply<CmdType, type>(iter, ctrl.reg0, ctrl.reg1, ctx);
      frame.regs_.set(GetOffset(frame.fn_, iter->read<Reg>()), result);
    });

  } else if constexpr (std::is_same_v<CmdType, VariantCmd> or
                       std::is_same_v<CmdType, TupleCmd>) {
    std::vector<type::Type const *> vals =
        internal::Deserialize<uint16_t, type::Type const *>(
            iter,
            [ctx](Reg reg) { return ctx->resolve<type::Type const *>(reg); });

    auto &frame = ctx->call_stack.top();
    frame.regs_.set(GetOffset(frame.fn_, iter->read<Reg>()),
                    CmdType::fn_ptr(std::move(vals)));

  } else if constexpr (std::is_same_v<CmdType, StoreCmd>) {
    auto ctrl = iter->read<typename CmdType::control_bits>();
    PrimitiveDispatch(ctrl.primitive_type, [&](auto tag) {
      using T = typename std::decay_t<decltype(tag)>::type;
      T val   = ctrl.reg ? ctx->resolve<T>(iter->read<Reg>()) : iter->read<T>();
      Addr addr = ctrl.reg_addr ? ctx->resolve<Addr>(iter->read<Reg>())
                                : iter->read<Addr>();
      static_assert(not std::is_same_v<T, void *>,
                    "Not handling addresses yet");
      switch (addr.kind) {
        case Addr::Kind::Stack:
          DEBUG_LOG("store")(addr);
          ctx->stack_.set(addr.as_stack, val);
          break;
        case Addr::Kind::ReadOnly:
          NOT_YET(
              "Storing into read-only data seems suspect. Is it just for "
              "initialization?");
          break;
        case Addr::Kind::Heap:
          *ASSERT_NOT_NULL(static_cast<T *>(addr.as_heap)) = val;
      }
    });

  } else if constexpr (std::is_same_v<CmdType, LoadCmd>) {
    auto &frame = ctx->call_stack.top();
    auto ctrl   = iter->read<typename CmdType::control_bits>();
    auto addr =
        ctrl.reg ? ctx->resolve<Addr>(iter->read<Reg>()) : iter->read<Addr>();
    auto result_reg = iter->read<Reg>();
    DEBUG_LOG("load")("addr = ", addr);
    DEBUG_LOG("load")("result_reg = ", result_reg);
    PrimitiveDispatch(ctrl.primitive_type, [&](auto tag) {
      using type = typename std::decay_t<decltype(tag)>::type;
      switch (addr.kind) {
        case Addr::Kind::Stack:
          frame.regs_.set(GetOffset(frame.fn_, result_reg),
                          ctx->stack_.get<type>(addr.as_stack));
          break;
        case Addr::Kind::ReadOnly: NOT_YET(); break;
        case Addr::Kind::Heap:
          frame.regs_.set(GetOffset(frame.fn_, result_reg),
                          *static_cast<type *>(addr.as_heap));
      }
    });

  } else if constexpr (std::is_same_v<CmdType, ArrowCmd>) {
    std::vector<type::Type const *> ins =
        internal::Deserialize<uint16_t, type::Type const *>(
            iter,
            [ctx](Reg reg) { return ctx->resolve<type::Type const *>(reg); });
    std::vector<type::Type const *> outs =
        internal::Deserialize<uint16_t, type::Type const *>(
            iter,
            [ctx](Reg reg) { return ctx->resolve<type::Type const *>(reg); });

    auto &frame = ctx->call_stack.top();
    frame.regs_.set(GetOffset(frame.fn_, iter->read<Reg>()),
                    type::Func(std::move(ins), std::move(outs)));

  } else if constexpr (std::is_same_v<CmdType, CallCmd>) {
    bool fn_is_reg                = iter->read<bool>();
    std::vector<bool> is_reg_bits = internal::ReadBits<uint16_t>(iter);

    AnyFunc f = fn_is_reg ? ctx->resolve<AnyFunc>(iter->read<Reg>())
                          : iter->read<AnyFunc>();
    type::Function const *fn_type = GetType(f);
    DEBUG_LOG("call")(f, ": ", fn_type->to_string());
    DEBUG_LOG("call")(is_reg_bits);

    iter->read<core::Bytes>();

    base::untyped_buffer call_buf;
    ASSERT(fn_type->input.size() == is_reg_bits.size());
    for (size_t i = 0; i < is_reg_bits.size(); ++i) {
      type::Type const *t = fn_type->input[i];
      if (is_reg_bits[i]) {
        auto reg = iter->read<Reg>();
        PrimitiveDispatch(PrimitiveIndex(t), [&](auto tag) {
          using type = typename std::decay_t<decltype(tag)>::type;
          call_buf.append(ctx->resolve<type>(reg));
        });

      } else if (t->is_big()) {
        NOT_YET();
      } else {
        PrimitiveDispatch(PrimitiveIndex(t), [&](auto tag) {
          using type = typename std::decay_t<decltype(tag)>::type;
          call_buf.append(iter->read<type>());
        });
      }
    }

    uint16_t num_rets = iter->read<uint16_t>();
    std::vector<Addr> return_slots;
    return_slots.reserve(num_rets);
    for (uint16_t i = 0; i < num_rets; ++i) {
      auto reg = iter->read<Reg>();
      // TODO: handle is_loc outparams.
      // NOTE: This is a hack using heap address slots to represent registers
      // since they are both void* and are used identically in the interpretter.
      auto addr = ir::Addr::Heap(ctx->call_stack.top().regs_.raw(
          ASSERT_NOT_NULL(ctx->call_stack.top().fn_->offset_or_null(reg))
              ->value()));
      DEBUG_LOG("call")("Ret addr = ", addr);
      return_slots.push_back(addr);
    }

    backend::Execute(f, call_buf, return_slots, ctx);

  } else if constexpr (std::is_same_v<CmdType, ReturnCmd>) {
    auto ctrl     = iter->read<typename CmdType::control_bits>();
    uint16_t n    = iter->read<uint16_t>();
    Addr ret_slot = ret_slots[n];

    if (ctrl.only_get) {
      auto reg    = iter->read<Reg>();
      auto &frame = ctx->call_stack.top();
      frame.regs_.set(GetOffset(frame.fn_, reg), ret_slot);
      return nullptr;
    }
    DEBUG_LOG("return")("return slot #", n, " = ", ret_slot);

    ASSERT(ret_slot.kind == Addr::Kind::Heap);
    PrimitiveDispatch(ctrl.primitive_type, [&](auto tag) {
      using T = typename std::decay_t<decltype(tag)>::type;
      T val   = ctrl.reg ? ctx->resolve<T>(iter->read<Reg>()) : iter->read<T>();
      DEBUG_LOG("return")("val = ", val);
      *ASSERT_NOT_NULL(static_cast<T *>(ret_slot.as_heap)) = val;
    });

  } else if constexpr (std::is_same_v<CmdType, PhiCmd>) {
    uint8_t primitive_type = iter->read<uint8_t>();
    uint16_t num           = iter->read<uint16_t>();
    uint64_t index         = std::numeric_limits<uint64_t>::max();
    for (uint16_t i = 0; i < num; ++i) {
      if (ctx->call_stack.top().prev_ == iter->read<BasicBlock const *>()) {
        index = i;
      }
    }
    ASSERT(index != std::numeric_limits<uint64_t>::max());

    auto &frame = ctx->call_stack.top();
    PrimitiveDispatch(primitive_type, [&](auto tag) {
      using T                = typename std::decay_t<decltype(tag)>::type;
      std::vector<T> results = internal::Deserialize<uint16_t, T>(
          iter, [ctx](Reg reg) { return ctx->resolve<T>(reg); });

      if constexpr (std::is_same_v<T, bool>) {
        frame.regs_.set(GetOffset(frame.fn_, iter->read<Reg>()),
                        bool{results[index]});
      } else {
        frame.regs_.set(GetOffset(frame.fn_, iter->read<Reg>()),
                        results[index]);
      }
    });

  } else if constexpr (std::is_same_v<CmdType, JumpCmd>) {
    switch (iter->read<typename CmdType::Kind>()) {
      case CmdType::Kind::kRet: return ReturnBlock();
      case CmdType::Kind::kUncond: return iter->read<BasicBlock const *>();
      case CmdType::Kind::kCond: {
        bool b           = ctx->resolve<bool>(iter->read<Reg>());
        auto false_block = iter->read<BasicBlock const *>();
        auto true_block  = iter->read<BasicBlock const *>();
        return b ? true_block : false_block;
      }
      case CmdType::Kind::kChoose:
        UNREACHABLE("Choose jumps can never be executed.");
      default: UNREACHABLE();
    }
  } else if constexpr (std::is_same_v<CmdType, ScopeCmd>) {
    NOT_YET();
    /*
    auto *compiler = iter->read<compiler::Compiler *>();

    std::vector<JumpHandler const *> inits =
        internal::Deserialize<uint16_t, JumpHandler const *>(
            iter,
            [ctx](Reg reg) { return ctx->resolve<JumpHandler const *>(reg); });
    std::vector<AnyFunc> dones = internal::Deserialize<uint16_t, AnyFunc>(
        iter, [ctx](Reg reg) { return ctx->resolve<AnyFunc>(reg); });

    auto num_blocks = iter->read<uint16_t>();
    absl::flat_hash_map<std::string_view, BlockDef *> blocks;
    for (uint16_t i = 0; i < num_blocks; ++i) {
      auto name  = iter->read<std::string_view>();
      auto block = ctx->resolve<BlockDef *>(iter->read<BlockDef *>());
      blocks.emplace(name, block);
    }

    Reg result_reg = iter->read<Reg>();
    auto &frame    = ctx->call_stack.top();
    frame.regs_.set(GetOffset(frame.fn_, result_reg),
                    compiler->AddScope(std::move(inits), std::move(dones),
                                       std::move(blocks)));

    */
  } else if constexpr (std::is_same_v<CmdType, BlockCmd>) {
    NOT_YET();
    /*
    auto *compiler                   = iter->read<compiler::Compiler *>();
    std::vector<AnyFunc> before_vals = internal::Deserialize<uint16_t, AnyFunc>(
        iter, [ctx](Reg reg) { return ctx->resolve<AnyFunc>(reg); });
    std::vector<JumpHandler const *> after_vals =
        internal::Deserialize<uint16_t, JumpHandler const *>(
            iter,
            [ctx](Reg reg) { return ctx->resolve<JumpHandler const *>(reg); });
    Reg result_reg = iter->read<Reg>();

    // TODO deal with leak.
    auto &frame = ctx->call_stack.top();
    frame.regs_.set(
        GetOffset(frame.fn_, result_reg),
        compiler->AddBlock(std::move(before_vals), std::move(after_vals)));

        */
  } else if constexpr (std::is_same_v<CmdType, EnumerationCmd>) {
    using enum_t             = typename CmdType::enum_t;
    bool is_enum_not_flags   = iter->read<bool>();
    uint16_t num_enumerators = iter->read<uint16_t>();
    uint16_t num_specified   = iter->read<uint16_t>();
    module::BasicModule *mod = iter->read<module::BasicModule *>();
    std::vector<std::pair<std::string_view, std::optional<enum_t>>> enumerators;
    enumerators.reserve(num_enumerators);
    for (uint16_t i = 0; i < num_enumerators; ++i) {
      enumerators.emplace_back(iter->read<std::string_view>(), std::nullopt);
    }

    absl::flat_hash_set<enum_t> vals;

    for (uint16_t i = 0; i < num_specified; ++i) {
      uint64_t index = iter->read<uint64_t>();
      enum_t val = iter->read<bool>() ? ctx->resolve<enum_t>(iter->read<Reg>())
                                      : iter->read<enum_t>();
      enumerators[i].second = val;
      vals.insert(val);
    }

    type::Type *result = nullptr;
    absl::BitGen gen;

    if (is_enum_not_flags) {
      for (auto & [ name, maybe_val ] : enumerators) {
        DEBUG_LOG("enum")(name, " => ", maybe_val);

        if (not maybe_val.has_value()) {
          bool success;
          enum_t x;
          do {
            x       = absl::Uniform<enum_t>(gen);
            success = vals.insert(x).second;
            DEBUG_LOG("enum")("Adding value ", x, " for ", name);
            maybe_val = x;
          } while (not success);
        }
      }
      absl::flat_hash_map<std::string, EnumVal> mapping;

      for (auto[name, maybe_val] : enumerators) {
        ASSERT(maybe_val.has_value() == true);
        mapping.emplace(std::string(name), EnumVal{maybe_val.value()});
      }
      DEBUG_LOG("enum")(vals, ", ", mapping);
      result = new type::Enum(mod, std::move(mapping));
    } else {
      for (auto & [ name, maybe_val ] : enumerators) {
        DEBUG_LOG("flags")(name, " => ", maybe_val);

        if (not maybe_val.has_value()) {
          bool success;
          enum_t x;
          do {
            x       = absl::Uniform<enum_t>(absl::IntervalClosedOpen, gen, 0,
                                      std::numeric_limits<enum_t>::digits);
            success = vals.insert(x).second;
            DEBUG_LOG("enum")("Adding value ", x, " for ", name);
            maybe_val = x;
          } while (not success);
        }
      }

      absl::flat_hash_map<std::string, FlagsVal> mapping;

      for (auto[name, maybe_val] : enumerators) {
        ASSERT(maybe_val.has_value() == true);
        mapping.emplace(std::string(name),
                        FlagsVal{enum_t{1} << maybe_val.value()});
      }

      DEBUG_LOG("flags")(vals, ", ", mapping);
      result = new type::Flags(mod, std::move(mapping));
    }

    auto &frame = ctx->call_stack.top();
    frame.regs_.set(GetOffset(frame.fn_, iter->read<Reg>()), result);

  } else if constexpr (std::is_same_v<CmdType, StructCmd>) {
    std::vector<std::tuple<std::string_view, type::Type const *>> fields;
    auto num = iter->read<uint16_t>();
    fields.reserve(num);
    auto *scope = iter->read<ast::Scope const *>();
    auto *mod   = iter->read<module::BasicModule *>();
    for (uint16_t i = 0; i < num; ++i) {
      fields.emplace_back(iter->read<std::string_view>(), nullptr);
    }

    size_t index = 0;
    internal::Deserialize<uint16_t, type::Type const *>(iter, [&](Reg reg) {
      std::get<1>(fields[index++]) = ctx->resolve<type::Type const *>(reg);
    });

    auto &frame = ctx->call_stack.top();
    frame.regs_.set(GetOffset(frame.fn_, iter->read<Reg>()),
                    new type::Struct(scope, mod, fields));

  } else if constexpr (std::is_same_v<CmdType, OpaqueTypeCmd>) {
    auto &frame = ctx->call_stack.top();
    auto *mod   = iter->read<module::BasicModule const *>();
    frame.regs_.set(GetOffset(frame.fn_, iter->read<Reg>()),
                    new type::Opaque(mod));

  } else if constexpr (std::is_same_v<CmdType, ArrayCmd>) {
    using length_t = typename CmdType::length_t;
    auto &frame    = ctx->call_stack.top();
    auto ctrl_bits = iter->read<typename CmdType::control_bits>();
    length_t len   = ctrl_bits.length_is_reg
                       ? ctx->resolve<length_t>(iter->read<Reg>())
                       : iter->read<length_t>();
    type::Type const *data_type =
        ctrl_bits.type_is_reg
            ? ctx->resolve<type::Type const *>(iter->read<Reg>())
            : iter->read<type::Type const *>();

    frame.regs_.set(GetOffset(frame.fn_, iter->read<Reg>()),
                    type::Arr(len, data_type));

#if defined(ICARUS_DEBUG)
  } else if constexpr (std::is_same_v<CmdType, DebugIrCmd>) {
    std::stringstream ss;
    ss << *ctx->call_stack.top().fn_;
    DEBUG_LOG()(ss.str());
#endif
  } else if constexpr (std::is_same_v<CmdType, CastCmd>) {
    auto to_type   = iter->read<uint8_t>();
    auto from_type = iter->read<uint8_t>();
    auto &frame    = ctx->call_stack.top();
    PrimitiveDispatch(from_type, [&](auto from_tag) {
      using FromType = typename std::decay_t<decltype(from_tag)>::type;
      [[maybe_unused]] auto val    = ctx->resolve<FromType>(iter->read<Reg>());
      [[maybe_unused]] auto offset = GetOffset(frame.fn_, iter->read<Reg>());
      if constexpr (std::is_integral_v<FromType>) {
        switch (to_type) {
          case PrimitiveIndex<int8_t>():
            frame.regs_.set(offset, static_cast<int8_t>(val));
            break;
          case PrimitiveIndex<int16_t>():
            frame.regs_.set(offset, static_cast<int16_t>(val));
            break;
          case PrimitiveIndex<int32_t>():
            frame.regs_.set(offset, static_cast<int32_t>(val));
            break;
          case PrimitiveIndex<int64_t>():
            frame.regs_.set(offset, static_cast<int64_t>(val));
            break;
          case PrimitiveIndex<uint8_t>():
            frame.regs_.set(offset, static_cast<uint8_t>(val));
            break;
          case PrimitiveIndex<uint16_t>():
            frame.regs_.set(offset, static_cast<uint16_t>(val));
            break;
          case PrimitiveIndex<uint32_t>():
            frame.regs_.set(offset, static_cast<uint32_t>(val));
            break;
          case PrimitiveIndex<uint64_t>():
            frame.regs_.set(offset, static_cast<uint64_t>(val));
            break;
          case PrimitiveIndex<float>():
            frame.regs_.set(offset, static_cast<float>(val));
            break;
          case PrimitiveIndex<double>():
            frame.regs_.set(offset, static_cast<double>(val));
            break;
          case PrimitiveIndex<EnumVal>():
            frame.regs_.set(offset, EnumVal(val));
            break;
          case PrimitiveIndex<FlagsVal>():
            frame.regs_.set(offset, FlagsVal(val));
            break;
        }
      } else if constexpr (std::is_floating_point_v<FromType>) {
        switch (to_type) {
          case PrimitiveIndex<float>():
            frame.regs_.set(offset, static_cast<float>(val));
            break;
          case PrimitiveIndex<double>():
            frame.regs_.set(offset, static_cast<double>(val));
            break;
        }
      } else if constexpr (std::is_same_v<FromType, EnumVal> or
                           std::is_same_v<FromType, FlagsVal>) {
        switch (to_type) {
          case PrimitiveIndex<int8_t>():
            frame.regs_.set(offset, static_cast<int8_t>(val.value));
            break;
          case PrimitiveIndex<int16_t>():
            frame.regs_.set(offset, static_cast<int16_t>(val.value));
            break;
          case PrimitiveIndex<int32_t>():
            frame.regs_.set(offset, static_cast<int32_t>(val.value));
            break;
          case PrimitiveIndex<int64_t>():
            frame.regs_.set(offset, static_cast<int64_t>(val.value));
            break;
          case PrimitiveIndex<uint8_t>():
            frame.regs_.set(offset, static_cast<uint8_t>(val.value));
            break;
          case PrimitiveIndex<uint16_t>():
            frame.regs_.set(offset, static_cast<uint16_t>(val.value));
            break;
          case PrimitiveIndex<uint32_t>():
            frame.regs_.set(offset, static_cast<uint32_t>(val.value));
            break;
          case PrimitiveIndex<uint64_t>():
            frame.regs_.set(offset, static_cast<uint64_t>(val.value));
            break;
        }
      } else if constexpr (std::is_pointer_v<FromType>) {
        NOT_YET(offset, val);
      } else {
        UNREACHABLE(offset, val);
      }
    });
  } else if constexpr (std::is_same_v<CmdType, SemanticCmd>) {
    ir::AnyFunc f;
    base::untyped_buffer call_buf(sizeof(ir::Addr));
    switch (iter->read<typename CmdType::Kind>()) {
      case CmdType::Kind::Init: {
        auto *t = iter->read<type::Type const *>();
        call_buf.append(ctx->resolve<Addr>(iter->read<Reg>()));

        if (auto *s = t->if_as<type::Struct>()) {
          f = s->init_func_;
        } else if (auto *tup = t->if_as<type::Tuple>()) {
          f = tup->init_func_.get();
        } else if (auto *a = t->if_as<type::Array>()) {
          f = a->init_func_.get();
        } else {
          NOT_YET();
        }
      } break;
      case CmdType::Kind::Destroy: {
        auto *t = iter->read<type::Type const *>();
        call_buf.append(ctx->resolve<Addr>(iter->read<Reg>()));

        if (auto *s = t->if_as<type::Struct>()) {
          f = s->destroy_func_.get();
        } else if (auto *tup = t->if_as<type::Tuple>()) {
          f = tup->destroy_func_.get();
        } else if (auto *a = t->if_as<type::Array>()) {
          f = a->destroy_func_.get();
        } else {
          NOT_YET();
        }
      } break;
      case CmdType::Kind::Move: {
        bool to_reg = iter->read<bool>();
        auto *t     = iter->read<type::Type const *>();
        call_buf.append(ctx->resolve<Addr>(iter->read<Reg>()));
        call_buf.append(to_reg ? ctx->resolve<Addr>(iter->read<Reg>())
                               : iter->read<Addr>());

        ir::AnyFunc f;
        if (auto *s = t->if_as<type::Struct>()) {
          f = s->move_assign_func_.get();
        } else if (auto *tup = t->if_as<type::Tuple>()) {
          f = tup->move_assign_func_.get();
        } else if (auto *a = t->if_as<type::Array>()) {
          f = a->move_assign_func_.get();
        } else {
          NOT_YET();
        }
      } break;
      case CmdType::Kind::Copy: {
        bool to_reg = iter->read<bool>();
        auto *t     = iter->read<type::Type const *>();
        call_buf.append(ctx->resolve<Addr>(iter->read<Reg>()));
        call_buf.append(to_reg ? ctx->resolve<Addr>(iter->read<Reg>())
                               : iter->read<Addr>());
        if (auto *s = t->if_as<type::Struct>()) {
          f = s->copy_assign_func_.get();
        } else if (auto *tup = t->if_as<type::Tuple>()) {
          f = tup->copy_assign_func_.get();
        } else if (auto *a = t->if_as<type::Array>()) {
          f = a->copy_assign_func_.get();
        } else {
          NOT_YET();
        }
      } break;
    }

    backend::Execute(f, call_buf, ret_slots, ctx);

  } else if constexpr (std::is_same_v<CmdType, LoadSymbolCmd>) {
    auto name = iter->read<std::string_view>();
    auto type = iter->read<type::Type const *>();
    auto reg  = iter->read<Reg>();
    void *sym = ASSERT_NOT_NULL(dlsym(RTLD_DEFAULT, std::string(name).c_str()));

    auto &frame = ctx->call_stack.top();
    if (type->is<type::Function>()) {
      frame.regs_.set(GetOffset(frame.fn_, reg),
                      ir::AnyFunc{ir::Foreign(sym, type)});
    } else if (type->is<type::Pointer>()) {
      frame.regs_.set(GetOffset(frame.fn_, reg),
                      ir::Addr::Heap(*static_cast<void **>(sym)));
    } else {
      NOT_YET(type->to_string());
    }
  } else if constexpr (std::is_same_v<CmdType, TypeInfoCmd>) {
    auto ctrl_bits = iter->read<uint8_t>();
    type::Type const *type =
        (ctrl_bits & 0x01) ? ctx->resolve<type::Type const *>(iter->read<Reg>())
                           : iter->read<type::Type const *>();
    auto reg = iter->read<Reg>();

    auto &frame = ctx->call_stack.top();
    if (ctrl_bits & 0x02) {
      frame.regs_.set(GetOffset(frame.fn_, reg),
                      type->alignment(core::Interpretter()));

    } else {
      frame.regs_.set(GetOffset(frame.fn_, reg),
                      type->bytes(core::Interpretter()));
    }

  } else if constexpr (std::is_same_v<CmdType, AccessCmd>) {
    auto ctrl_bits   = iter->read<typename CmdType::control_bits>();
    auto const *type = iter->read<type::Type const *>();

    Addr addr = ctrl_bits.reg_ptr ? ctx->resolve<Addr>(iter->read<Reg>())
                                  : iter->read<Addr>();
    int64_t index = ctrl_bits.reg_index
                        ? ctx->resolve<int64_t>(iter->read<Reg>())
                        : iter->read<int64_t>();
    auto reg = iter->read<Reg>();

    auto arch = core::Interpretter();
    core::Bytes offset;
    if (ctrl_bits.is_array) {
      offset = core::FwdAlign(type->bytes(arch), type->alignment(arch)) * index;
    } else if (auto *struct_type = type->if_as<type::Struct>()) {
      offset = struct_type->offset(index, arch);
    } else if (auto *tuple_type = type->if_as<type::Tuple>()) {
      offset = struct_type->offset(index, arch);
    }

    auto &frame = ctx->call_stack.top();
    frame.regs_.set(GetOffset(frame.fn_, reg), addr + offset);
  } else if constexpr (std::is_same_v<CmdType, VariantAccessCmd>) {
    auto &frame  = ctx->call_stack.top();
    bool get_val = iter->read<bool>();
    bool is_reg  = iter->read<bool>();

    Addr addr =
        is_reg ? ctx->resolve<Addr>(iter->read<Reg>()) : iter->read<Addr>();
    DEBUG_LOG("variant")(addr);
    if (get_val) {
      auto const *variant = iter->read<type::Variant const *>();
      DEBUG_LOG("variant")(variant);
      DEBUG_LOG("variant")(variant->to_string());
      auto arch = core::Interpretter();
      addr += core::FwdAlign(type::Type_->bytes(arch),
                             variant->alternative_alignment(arch));
      DEBUG_LOG("variant")(variant->to_string());
      DEBUG_LOG("variant")(addr);
    }

    Reg reg = iter->read<Reg>();
    DEBUG_LOG("variant")(reg);
    frame.regs_.set(GetOffset(frame.fn_, reg), addr);
  }
  return nullptr;
}

}  // namespace ir

namespace backend {

void CallForeignFn(ir::Foreign const &f, base::untyped_buffer const &arguments,
                   std::vector<ir::Addr> const &ret_slots,
                   base::untyped_buffer *stack);

void Execute(ir::AnyFunc fn, base::untyped_buffer const &arguments,
             std::vector<ir::Addr> const &ret_slots, ExecContext *ctx) {
  if (fn.is_fn()) {
    Execute(fn.func(), arguments, ret_slots, ctx);
  } else {
    CallForeignFn(fn.foreign(), arguments, ret_slots, &ctx->stack_);
  }
}

void Execute(ir::CompiledFn *fn, const base::untyped_buffer &arguments,
             const std::vector<ir::Addr> &ret_slots, ExecContext *ctx) {
  // TODO: Understand why and how work-items may not be complete and add an
  // explanation here. I'm quite confident this is really possible with the
  // generics model I have, but I can't quite articulate exactly why it only
  // happens for generics and nothing else.
  if (fn->work_item and *fn->work_item) { (std::move(*fn->work_item))(); }

  // TODO what about bound constants?
  ctx->call_stack.emplace(fn, arguments, ctx);

  // TODO log an error if you're asked to execute a function that had an
  // error.

  auto arch   = core::Interpretter();
  auto offset = core::Bytes{0};
  DEBUG_LOG("dbg")(fn);
  DEBUG_LOG("dbg")(fn->type_);
  DEBUG_LOG("dbg")(fn->type_->to_string());
  for (auto *t : fn->type_->output) {
    DEBUG_LOG("dbg")(t->to_string());
    offset = core::FwdAlign(offset, t->alignment(arch)) + t->bytes(arch);
  }
  base::untyped_buffer ret_buffer(offset.value());

  while (true) {
    auto const *block = ctx->ExecuteBlock(ret_slots);
    if (block == ir::ReturnBlock()) {
      ctx->call_stack.pop();
      return;
    } else {
      ctx->call_stack.top().MoveTo(block);
    }
  }
}

ExecContext::ExecContext() : stack_(50u) {}

ir::BasicBlock const *ExecContext::current_block() {
  return call_stack.top().current_;
}

ExecContext::Frame::Frame(ir::CompiledFn *fn,
                          const base::untyped_buffer &arguments,
                          ExecContext *ctx)
    : fn_(fn),
      current_(fn_->entry()),
      prev_(fn_->entry()),
      regs_(fn_->MakeBuffer()) {
  regs_.write(0, arguments);

  auto arch = core::Interpretter();
  fn->allocs().for_each([&](type::Type const *t, ir::Reg r) {
    core::Bytes offset = *ASSERT_NOT_NULL(fn_->offset_or_null(r));
    DEBUG_LOG("allocs")
    ("Allocating type = ", t->to_string(), ", reg = ", r,
     ", offset = ", offset);
    regs_.set(offset.value(),
              ir::Addr::Stack(core::FwdAlign(core::Bytes{ctx->stack_.size()},
                                             t->alignment(arch))
                                  .value()));

    ctx->stack_.append_bytes(t->bytes(arch).value(),
                             t->alignment(arch).value());
  });
}

#if defined(ICARUS_DEBUG)
#define DEBUG_CASES CASE(DebugIrCmd);
#else
#define DEBUG_CASES
#endif

#define CASES                                                                  \
  DEBUG_CASES                                                                  \
  CASE(PrintCmd);                                                              \
  CASE(AddCmd);                                                                \
  CASE(SubCmd);                                                                \
  CASE(MulCmd);                                                                \
  CASE(DivCmd);                                                                \
  CASE(ModCmd);                                                                \
  CASE(NegCmd);                                                                \
  CASE(NotCmd);                                                                \
  CASE(LtCmd);                                                                 \
  CASE(LeCmd);                                                                 \
  CASE(EqCmd);                                                                 \
  CASE(NeCmd);                                                                 \
  CASE(GeCmd);                                                                 \
  CASE(GtCmd);                                                                 \
  CASE(StoreCmd);                                                              \
  CASE(LoadCmd);                                                               \
  CASE(VariantCmd);                                                            \
  CASE(TupleCmd);                                                              \
  CASE(ArrowCmd);                                                              \
  CASE(PtrCmd);                                                                \
  CASE(BufPtrCmd);                                                             \
  CASE(JumpCmd);                                                               \
  CASE(XorFlagsCmd);                                                           \
  CASE(AndFlagsCmd);                                                           \
  CASE(OrFlagsCmd);                                                            \
  CASE(CastCmd);                                                               \
  CASE(RegisterCmd);                                                           \
  CASE(ReturnCmd);                                                             \
  CASE(EnumerationCmd);                                                        \
  CASE(StructCmd);                                                             \
  CASE(OpaqueTypeCmd);                                                         \
  CASE(SemanticCmd);                                                           \
  CASE(LoadSymbolCmd);                                                         \
  CASE(TypeInfoCmd);                                                           \
  CASE(AccessCmd);                                                             \
  CASE(VariantAccessCmd);                                                      \
  CASE(CallCmd);                                                               \
  CASE(BlockCmd);                                                              \
  CASE(ScopeCmd)



ir::BasicBlock const *ExecContext::ExecuteBlock(
    const std::vector<ir::Addr> &ret_slots) {
  auto iter = current_block()->cmd_buffer_.begin();
  while (true) {
    ASSERT(iter < buf_.end());
    auto cmd_index = iter.read<ir::cmd_index_t>();
    switch (cmd_index) {
#define CASE(type)                                                             \
  case ir::type::index: {                                                      \
    DEBUG_LOG("dbg")(#type);                                                   \
    if (auto blk = ir::ExecuteCmd<ir::type>(&iter, ret_slots, this)) {         \
      return blk;                                                              \
    }                                                                          \
  } break
      CASES;
#undef CASE
      default: UNREACHABLE(static_cast<int>(cmd_index));
    }
  }
}

#undef CASE
#undef CASES

template <typename T>
static T LoadValue(ir::Addr addr, base::untyped_buffer const &stack) {
  switch (addr.kind) {
    case ir::Addr::Kind::Heap:
      return *ASSERT_NOT_NULL(static_cast<T *>(addr.as_heap));
    case ir::Addr::Kind::Stack: return stack.get<T>(addr.as_stack);
    case ir::Addr::Kind::ReadOnly:
      return ir::ReadOnlyData.get<T>(addr.as_rodata);
  }
  UNREACHABLE(DUMP(static_cast<int>(addr.kind)));
}

template <typename T>
static void StoreValue(T val, ir::Addr addr, base::untyped_buffer *stack) {
  if constexpr (std::is_same_v<std::decay_t<T>, void *>) {
    auto start  = reinterpret_cast<uintptr_t>(stack->raw(0));
    auto end    = reinterpret_cast<uintptr_t>(stack->raw(stack->size()));
    auto result = reinterpret_cast<uintptr_t>(val);
    if (start <= result and result < end) {
      StoreValue(ir::Addr::Stack(result - start), addr, stack);
    } else {
      StoreValue(ir::Addr::Heap(val), addr, stack);
    }
    // TODO readonly data
  } else {
    switch (addr.kind) {
      case ir::Addr::Kind::Stack: stack->set(addr.as_stack, val); return;
      case ir::Addr::Kind::ReadOnly: UNREACHABLE();
      case ir::Addr::Kind::Heap:
        *ASSERT_NOT_NULL(static_cast<T *>(addr.as_heap)) = val;
    }
  }
}

template <size_t N, typename... Ts>
struct RetrieveArgs;

template <size_t N, typename T, typename... Ts>
struct RetrieveArgs<N, T, Ts...> {
  template <typename OutTup>
  void operator()(base::untyped_buffer const &arguments, size_t *index,
                  OutTup *out_tup) {
    if constexpr (std::is_same_v<T, void *>) {
      // Any pointer-type alignment is fine. (TODO they're always the same,
      // right?)
      *index    = ((*index - 1) | (alignof(ir::Addr) - 1)) + 1;
      auto addr = arguments.get<ir::Addr>(*index);
      void *ptr = nullptr;
      switch (addr.kind) {
        case ir::Addr::Kind::Stack: NOT_YET(addr.as_stack);
        case ir::Addr::Kind::Heap:
          ptr = static_cast<void *>(addr.as_heap);
          break;
        case ir::Addr::Kind::ReadOnly:
          ptr = static_cast<void *>(ir::ReadOnlyData.raw(addr.as_rodata));
          break;
        default: UNREACHABLE(static_cast<int>(addr.kind));
      }
      std::get<N>(*out_tup) = ptr;
      *index += sizeof(ir::Addr);
      RetrieveArgs<N + 1, Ts...>{}(arguments, index, out_tup);
    } else {
      auto arch = core::Interpretter();
      auto t    = type::Get<T>();
      *index = core::FwdAlign(core::Bytes{*index}, t->alignment(arch)).value();
      std::get<N>(*out_tup) = arguments.get<T>(*index);
      *index += t->bytes(arch).value();
      RetrieveArgs<N + 1, Ts...>{}(arguments, index, out_tup);
    }
  }
};

template <size_t N>
struct RetrieveArgs<N> {
  template <typename OutTup>
  void operator()(base::untyped_buffer const &arguments, size_t *index,
                  OutTup *out_tup) {}
};

template <typename... Ts>
std::tuple<Ts...> MakeTupleArgs(base::untyped_buffer const &arguments) {
  std::tuple<Ts...> results;
  size_t index = 0;
  RetrieveArgs<0, Ts...>{}(arguments, &index, &results);
  return results;
}

// TODO Generalize this based on calling convention.
template <typename Out, typename... Ins>
void FfiCall(ir::Foreign const &f, base::untyped_buffer const &arguments,
             std::vector<ir::Addr> const *ret_slots,
             base::untyped_buffer *stack) {
  using fn_t = Out (*)(Ins...);
  fn_t fn    = (fn_t)(f.get());

  if constexpr (std::is_same_v<Out, void>) {
    std::apply(fn, MakeTupleArgs<Ins...>(arguments));
  } else {
    StoreValue(std::apply(fn, MakeTupleArgs<Ins...>(arguments)),
               ret_slots->at(0), stack);
  }
}

void CallForeignFn(ir::Foreign const &f, base::untyped_buffer const &arguments,
                   std::vector<ir::Addr> const &ret_slots,
                   base::untyped_buffer *stack) {
  // TODO we can catch locally or in the same lexical scope stack if a
  // redeclaration of the same foreign symbol has a different type.
  // TODO handle failures gracefully.
  // TODO Consider caching these.
  // TODO Handle a bunch more function types in a coherent way.
  auto *fn_type = &f.type()->as<type::Function>();
  if (fn_type == type::Func({type::Int64}, {type::Int64})) {
    FfiCall<int64_t, int64_t>(f, arguments, &ret_slots, stack);
  } else if (fn_type == type::Func({type::Int64}, {})) {
    FfiCall<void, int64_t>(f, arguments, &ret_slots, stack);
  } else if (fn_type == type::Func({type::Float64}, {type::Float64})) {
    FfiCall<double, double>(f, arguments, &ret_slots, stack);
  } else if (fn_type == type::Func({type::Float32}, {type::Float32})) {
    FfiCall<float, float>(f, arguments, &ret_slots, stack);
  } else if (fn_type == type::Func({}, {type::Int64})) {
    FfiCall<int64_t>(f, arguments, &ret_slots, stack);
  } else if (fn_type == type::Func({type::Nat8}, {type::Int64})) {
    FfiCall<int64_t, uint8_t>(f, arguments, &ret_slots, stack);
  } else if (fn_type->input.size() == 1 and
             fn_type->input[0]->is<type::Pointer>() and
             fn_type->output.size() == 1 and
             fn_type->output[0] == type::Int64) {
    FfiCall<int64_t, void *>(f, arguments, &ret_slots, stack);
  } else if (fn_type->input.size() == 2 and
             fn_type->input[0]->is<type::Pointer>() and
             fn_type->input[1]->is<type::Pointer>() and
             fn_type->output.size() == 1 and
             fn_type->output[0]->is<type::Pointer>()) {
    FfiCall<void *, void *, void *>(f, arguments, &ret_slots, stack);
  } else if (fn_type->input.size() == 2 and fn_type->input[0] == type::Int64 and
             fn_type->input[1]->is<type::Pointer>() and
             fn_type->output.size() == 1 and
             fn_type->output[0] == type::Int64) {
    FfiCall<int64_t, int64_t, void *>(f, arguments, &ret_slots, stack);
  } else if (fn_type->input.size() == 1 and fn_type->input[0] == type::Int64 and
             fn_type->output.size() == 1 and
             fn_type->output[0]->is<type::Pointer>()) {
    FfiCall<void *, int64_t>(f, arguments, &ret_slots, stack);
  } else if (fn_type->input.size() == 1 and fn_type->input[0] == type::Nat64 and
             fn_type->output.size() == 1 and
             fn_type->output[0]->is<type::Pointer>()) {
    FfiCall<void *, uint64_t>(f, arguments, &ret_slots, stack);
  } else if (fn_type->input.size() == 1 and fn_type->output.empty() and
             fn_type->input[0]->is<type::Pointer>()) {
    FfiCall<void, void *>(f, arguments, &ret_slots, stack);
  } else {
    UNREACHABLE(fn_type->to_string());
  }
}

}  // namespace backend
