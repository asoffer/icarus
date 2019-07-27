#ifndef ICARUS_IR_CMD_TYPES_H
#define ICARUS_IR_CMD_TYPES_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "ir/basic_block.h"
#include "ir/cmd/util.h"
#include "ir/cmd_buffer.h"
#include "ir/reg.h"
#include "type/tuple.h"
#include "type/variant.h"
#include "type/pointer.h"

struct Module;

namespace ir {
namespace internal {

template <typename R, typename ArgTup>
struct FunctionPointer {};
template <typename R, typename... Args>
struct FunctionPointer<R, std::tuple<Args...>> {
  using type = R (*)(Args...);
};

template <typename R, typename ArgTup>
using FunctionPointerT = typename FunctionPointer<R, ArgTup>::type;

template <typename R, typename ArgTup, FunctionPointerT<R, ArgTup> FnPtr>
struct Functor {
  template <typename... Args>
  R operator()(Args... args) const {
    return FnPtr(std::forward<Args>(args)...);
  }
};

}  // namespace internal

using VariantCmd = internal::VariadicCmd<16, type::Type const*, type::Var>;
using TupleCmd   = internal::VariadicCmd<17, type::Type const *, type::Tup>;
using PtrCmd     = internal::UnaryCmd<
    18,
    internal::Functor<type::Pointer const *, std::tuple<type::Type const *>,
                      type::Ptr>,
    type::Type const *>;
using BufPtrCmd = internal::UnaryCmd<
    19,
    internal::Functor<type::BufferPointer const *,
                      std::tuple<type::Type const *>, type::BufPtr>,
    type::Type const *>;
// using ArrowCmd   = internal::BinaryCmd<18, /*std::equal_to<>*/, type::Type const*>;

inline RegOr<type::Type const *> Var(
    absl::Span<RegOr<type::Type const *> const> types) {
  return internal::MakeVariadicImpl<VariantCmd>(types);
}

inline RegOr<type::Type const *> Tup(
    absl::Span<RegOr<type::Type const *> const> types) {
  return internal::MakeVariadicImpl<TupleCmd>(types);
}

constexpr inline auto Ptr    = internal::UnaryHandler<PtrCmd>{};
constexpr inline auto BufPtr = internal::UnaryHandler<BufPtrCmd>{};

// struct EnumCmd {
//   constexpr static cmd_index_t index = 15;
// 
//   enum class : uint8_t ValueKind{kIsReg, kIsVal, kNone};
// 
//   static std::optional<BlockIndex> Execute(base::untyped_buffer::iterator* iter,
//                                            std::vector<Addr> const& ret_slots,
//                                            backend::ExecContext* ctx) {
//     bool is_enum = iter->read<bool>();
//     auto* mod    = iter->read<::Module*>();
//     auto num     = iter->read<size_t>();
//     absl::flat_hash_map<std::string, int32_t> members;
//     // Used for rejection sampling. At some point we should optimize this when
//     // the rejection probability gets high (if, e.g., a small enum is chosen and
//     // most values are taken.
//     absl::flat_hash_set<int32_t> taken;
//     std::vector<std::string_view> to_be_inserted;
//     for (size_t i = 0; i < num; ++i) {
//       auto name = iter->read<std::string_view>();
//       switch (iter->read<ValueKind>()) {
//         case ValueKind::kIsReg: {
//           int32_t val  = ctx->resolve<int32_t>(iter->read<Reg>());
//           bool success = taken.insert(val).second;
//           ASSERT(success == true);
//           members.emplace(std::string{name}, val);
//         } break;
//         case ValueKind::kIsValue: {
//           auto val = iter->read<int32_t>();
//           members.emplace(std::string{name}, val);
//           bool success = taken.insert(val).second;
//           ASSERT(success == true);
//         } break;
//         case ValueKind::kNone: to_be_inserted.push_back(name); break;
//       }
//     }
// 
//     absl::BitGen gen;
//     for (std::string_view enumerator : to_be_inserted) {
//       std::string name = s;
//       int32_t x;
//       {
//       try_again:
//         x = absl::Uniform(gen, std::numeric_limits<int32_t>::lowest(),
//                           std::numeric_limits<int32_t>::max());
// 
//         bool success = taken.insert(x).second;
//         if (!success) { goto try_again; }
//       }
//       members.emplace(x, s);
//     }
// 
//     // TODO flags case too!
//     auto& frame = ctx->call_stack.top();
//     frame.regs_.set(GetOffset(frame.fn_, iter->read<Reg>()),
//                     new type::Enum(mod, std::move(members)));
//     return std::nullopt;
//   }
// };
// 
// namespace internal {
// inline Reg MakeImpl(
//     bool is_enum, ::Module* mod,
//     absl::Span<std::pair<std::string_view, std::optional<RegOr<int32_t>>>>
//         enumerators) {
//   auto& blk = GetBlock();
//   blk.cmd_buffer_.append_index<EnumCmd>();
//   blk.cmd_buffer_.append(is_enum);
//   blk.cmd_buffer_.append(mod);
//   blk.cmd_buffer_.append<size_t>(enumerators.size());
//   for (auto const & [ name, val ] : enumerators) {
//     blk.cmd_buffer_.append(name);
//     if (val.has_value()) {
//       if (val->is_reg_) {
//         blk.cmd_buffer_.append(EnumCmd::ValueKind::kIsReg);
//       } else {
//         blk.cmd_buffer_.append(EnumCmd::ValueKind::kVal);
//       }
//     } else {
//       blk.cmd_buffer_.append(EnumCmd::ValueKind::kNone);
//     }
//     blk.cmd_buffer_.append(name);
//   }
//   Reg result = MakeResult(type::Get<T>());
//   blk.cmd_buffer_.append(result);
// }
// 
// inline Reg MakeEnum(
//     ::Module* mod,
//     absl::Span<std::pair<std::string_view, std::optional<RegOr<int32_t>>>>
//         enumerators) {
//   return MakeImpl(true, mod, enumerators);
// }
// 
// inline Reg MakeFlags(
//     ::Module* mod,
//     absl::Span<std::pair<std::string_view, std::optional<RegOr<int32_t>>>>
//         enumerators) {
//   return MakeImpl(false, mod, enumerators);
// }
}  // namespace ir

#endif  // ICARUS_IR_CMD_TYPES_H
