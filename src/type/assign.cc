#include "type/all.h"

#include "architecture.h"
#include "context.h"
#include "ir/components.h"
#include "ir/func.h"
#include "module.h"

// TODO destructor for previously held value.
// TODO here and everywhere else: choose a canonical module to add these
// fucntions to.

namespace type {
using base::check::Is;
void Array::EmitAssign(const Type *from_type, ir::Val from, ir::Register to,
                       Context *ctx) const {
  ASSERT(from_type, Is<Array>());
  auto *from_array_type = &from_type->as<Array>();

  std::unique_lock lock(mtx_);
  auto *&fn = assign_fns_[from_array_type];
  if (fn == nullptr) {
    fn = ctx->mod_->AddFunc(
        type::Func({from_type, type::Ptr(this)}, {}),
        base::vector<std::pair<std::string, ast::Expression *>>{
            {"from", nullptr}, {"to", nullptr}});

    CURRENT_FUNC(fn) {
      ir::BasicBlock::Current = fn->entry();
      auto val                = fn->Argument(0);
      auto var                = fn->Argument(1);

      auto len = [&]() -> ir::RegisterOr<i32> {
        if (from_array_type->fixed_length) {
          return static_cast<i32>(from_array_type->len);
        }
        return ir::LoadInt(ir::ArrayLength(val));
      }();

      auto *from_ptr_type   = type::Ptr(from_type->as<type::Array>().data_type);
      ir::Register from_ptr = ir::Index(from_type, val, 0);
      ir::Register from_end_ptr = ir::PtrIncr(from_ptr, len, from_ptr_type);

      if (!fixed_length) {
        ComputeDestroyWithoutLock(ctx);
        ir::LongArgs call_args;
        call_args.append(var);
        call_args.type_ = destroy_func_->type_;
        ir::Call(ir::AnyFunc{destroy_func_}, std::move(call_args));

        // TODO Architecture dependence?
        auto ptr = ir::Malloc(
            data_type, Architecture::InterprettingMachine().ComputeArrayLength(
                           len, data_type));
        ir::StoreInt(len, ir::ArrayLength(var));
        ir::StoreAddr(ptr, ir::ArrayData(var, type::Ptr(this)));
      }

      auto *to_ptr_type   = type::Ptr(data_type);
      ir::Register to_ptr = ir::Index(type::Ptr(this), var, 0);

      using tup =
          std::tuple<ir::RegisterOr<ir::Addr>, ir::RegisterOr<ir::Addr>>;
      ir::CreateLoop(
          [&](tup const &phis) {
            ASSERT(std::get<0>(phis).is_reg_);
            return ir::EqAddr(std::get<0>(phis).reg_, from_end_ptr);
          },
          [&](tup const &phis) {
            ASSERT(std::get<0>(phis).is_reg_);
            ASSERT(std::get<1>(phis).is_reg_);

            ir::Register ptr_fixed_reg =
                from_type->as<type::Array>().data_type->is_big()
                    ? std::get<0>(phis).reg_
                    : ir::Load(std::get<0>(phis).reg_, data_type);
            auto ptr_fixed_type =
                !from_type->as<type::Array>().data_type->is_big()
                    ? from_type->as<type::Array>().data_type
                    : type::Ptr(from_type->as<type::Array>().data_type);

            EmitCopyInit(from_array_type->data_type, data_type,
                         ir::Val::Reg(ptr_fixed_reg, ptr_fixed_type),
                         std::get<1>(phis).reg_, ctx);
            return std::make_tuple(
                ir::RegisterOr<ir::Addr>{
                    ir::PtrIncr(std::get<0>(phis).reg_, 1, from_ptr_type)},
                ir::RegisterOr<ir::Addr>{
                    ir::PtrIncr(std::get<1>(phis).reg_, 1, to_ptr_type)});
          },
          std::tuple<type::Type const *, type::Type const *>{from_ptr_type,
                                                             to_ptr_type},
          tup{ir::RegisterOr<ir::Addr>{from_ptr},
              ir::RegisterOr<ir::Addr>{to_ptr}});
      ir::ReturnJump();
    }
  }

  ir::LongArgs call_args;
  call_args.append(from);
  call_args.append(to);
  call_args.type_ = fn->type_;
  ir::Call(ir::AnyFunc{fn}, std::move(call_args));
}

void Pointer::EmitAssign(const Type *from_type, ir::Val from, ir::Register to,
                         Context *ctx) const {
  ASSERT(this == from_type);
  ir::StoreAddr(from.reg_or<ir::Addr>(), to);
}

void Enum::EmitAssign(const Type *from_type, ir::Val from, ir::Register to,
                      Context *ctx) const {
  ASSERT(this == from_type);
  ir::StoreEnum(from.reg_or<ir::EnumVal>(), to);
}

void Flags::EmitAssign(const Type *from_type, ir::Val from, ir::Register to,
                       Context *ctx) const {
  ASSERT(this == from_type);
  ir::StoreFlags(from.reg_or<ir::FlagsVal>(), to);
}

void Variant::EmitAssign(const Type *from_type, ir::Val from, ir::Register to,
                         Context *ctx) const {
  if (from_type->is<Variant>()) {
    // TODO find the best match for variant types. For instance, we allow
    // assignments like:
    // [3; int] | [4; bool] -> [--; int] | [--; bool]
    auto actual_type =
        ir::LoadType(ir::VariantType(std::get<ir::Register>(from.value)));
    auto landing = ir::Func::Current->AddBlock();
    for (const Type *v : from_type->as<Variant>().variants_) {
      auto next_block = ir::Func::Current->AddBlock();
      ir::BasicBlock::Current =
          ir::EarlyExitOn<false>(next_block, ir::EqType(actual_type, v));
      ir::StoreType(v, ir::VariantType(to));
      v->EmitAssign(
          v,
          ir::Val::Reg(
              ir::PtrFix(
                  ir::VariantValue(v, std::get<ir::Register>(from.value)), v),
              v),
          ir::VariantValue(v, to), ctx);
      ir::UncondJump(landing);
      ir::BasicBlock::Current = next_block;
    }
    ir::UncondJump(landing);
    ir::BasicBlock::Current = landing;
  } else {
    ir::StoreType(from_type, ir::VariantType(to));
    // TODO Find the best match amongst the variants available.
    const Type *best_match = from_type;
    best_match->EmitAssign(from_type, from, ir::VariantValue(best_match, to),
                           ctx);
  }
}

void Struct::EmitAssign(const Type *from_type, ir::Val from, ir::Register to,
                        Context *ctx) const {
  std::unique_lock lock(mtx_);
  ASSERT(this == from_type);
  if (!assign_func) {
    assign_func = ctx->mod_->AddFunc(
        type::Func({from_type, type::Ptr(this)}, {}),
        base::vector<std::pair<std::string, ast::Expression *>>{
            {"from", nullptr}, {"to", nullptr}});

    CURRENT_FUNC(assign_func) {
      ir::BasicBlock::Current = assign_func->entry();
      auto val                = assign_func->Argument(0);
      auto var                = assign_func->Argument(1);

      for (size_t i = 0; i < fields_.size(); ++i) {
        auto *field_type = from_type->as<type::Struct>().fields_.at(i).type;
        fields_[i].type->EmitAssign(
            fields_[i].type,
            ir::Val::Reg(ir::PtrFix(ir::Field(val, this, i), field_type),
                         field_type),
            ir::Field(var, this, i), ctx);
      }

      ir::ReturnJump();
    }
  }
  ASSERT(assign_func != nullptr);
  ir::LongArgs call_args;
  call_args.append(from);
  call_args.append(to);
  call_args.type_ = assign_func->type_;
  ir::Call(ir::AnyFunc{assign_func}, std::move(call_args));
}

void Function::EmitAssign(const Type *from_type, ir::Val from, ir::Register to,
                          Context *ctx) const {
  ASSERT(this == from_type);
  ir::StoreFunc(from.reg_or<ir::Func *>(), to);
}
void Primitive::EmitAssign(const Type *from_type, ir::Val from, ir::Register to,
                           Context *ctx) const {
  ASSERT(this == from_type);
  switch (this->type_) {
    case PrimType::Err: UNREACHABLE(this, ": Err");
    case PrimType::Type_:
      ir::StoreType(from.reg_or<type::Type const *>(), to);
      break;
    case PrimType::NullPtr: UNREACHABLE();
    case PrimType::EmptyArray: UNREACHABLE();
    case PrimType::Bool: ir::StoreBool(from.reg_or<bool>(), to); break;
    case PrimType::Char: ir::StoreChar(from.reg_or<char>(), to); break;
    case PrimType::Int: ir::StoreInt(from.reg_or<i32>(), to); break;
    case PrimType::Real: ir::StoreReal(from.reg_or<double>(), to); break;
    default: UNREACHABLE();
  }
}

void CharBuffer::EmitAssign(const Type *from_type, ir::Val from,
                            ir::Register to, Context *ctx) const {
  // TODO Only callable at compile-time?
  NOT_YET();
}
}  // namespace type
