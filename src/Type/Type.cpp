#ifndef ICARUS_UNITY
#include "Type.h"
#endif

extern llvm::Module *global_module;

extern llvm::Value *GetFunctionReferencedIn(Scope *scope,
                                            const std::string &fn_name,
                                            Function *fn_type);
extern IR::Func *GetFuncReferencedIn(Scope *scope, const std::string &fn_name,
                                     Function *fn_type);

namespace cstdlib {
extern llvm::Constant *malloc();
extern llvm::Constant *free();
} // namespace cstdlib

namespace data {
extern llvm::ConstantInt *const_uint(size_t n);
extern llvm::Constant *str(const std::string &s);
} // namespace data

size_t Type::bytes() const {
  // TODO make this platform specific
  if (this == Type_ || this == Void) { return 0; }
  if (this == Bool || this == Char) { return 1; }
  if (is_enum()) { return 4; }
  if (this == Int || this == Uint || this == Real || is_pointer()) { return 8; }
  if (is_array()) {
    auto array_type = (Array *)this;
    if (array_type->fixed_length) {
      return MoveForwardToAlignment(
          array_type->data_type->bytes() * array_type->len, alignment());
    } else {
      return 16;
    }
  }
  if (is_struct()) {
    auto struct_type = (Structure *)this;
    size_t num_bytes = 0;
    for (auto ft : struct_type->field_type) {
      num_bytes += ft->bytes();
      num_bytes = MoveForwardToAlignment(num_bytes, ft->alignment());
    }

    return MoveForwardToAlignment(num_bytes, alignment());
  }

  // kludgy. Should be fixed by making this uncallable
  if (is_type_variable()) { return 0; }

  std::cerr << *this << std::endl;
  NOT_YET;
}

size_t Type::alignment() const {
  // TODO make this platform specific
  if (this == Type_ || this == Void) { return 0; }
  if (this == Bool || this == Char) { return 1; }
  if (is_enum()) { return 4; }
  if (this == Int || this == Uint || this == Real || is_pointer() || is_function()) { return 8; }
  if (is_array()) {
    auto array_type = (Array *)this;
    if (array_type->fixed_length) {
      return array_type->data_type->alignment();
    } else {
      return 8;
    }
  }
  if (is_struct()) {
    auto struct_type = (Structure *)this;
    size_t alignment_val = 0;
    for (auto ft : struct_type->field_type) {
      auto a = ft->alignment();
      if (alignment_val <= a) { alignment_val = a; }
    }
    return alignment_val;
  }

  // kludgy. Should be fixed by making this uncallable
  if (is_type_variable()) { return 0; }

  std::cerr << *this << std::endl;
  NOT_YET;
}

void Type::IR_CallAssignment(Scope *scope, Type *lhs_type, Type *rhs_type,
                             IR::Value from_val, IR::Value to_var) {
  assert(scope);
  if (lhs_type->is_primitive() || lhs_type->is_pointer() ||
      lhs_type->is_enum()) {
    assert(lhs_type == rhs_type);
    IR::Store(rhs_type, from_val, to_var);

  } else if (lhs_type->is_array()) {
    assert(rhs_type->is_array());
    auto lhs_array_type = (Array *)lhs_type;
    if (lhs_array_type->fixed_length) {
      // Implies rhs has fixed length of same length as well.

      // TODO move this into a real Array method.
      for (size_t i = 0; i < lhs_array_type->len; ++i) {
        auto to_ptr =
            IR::Access(lhs_array_type->data_type, IR::Value(i), to_var);
        auto from_ptr =
            IR::Access(lhs_array_type->data_type, IR::Value(i), from_val);
        auto from_val = IR::Load(lhs_array_type->data_type, from_ptr);

        IR_CallAssignment(scope, lhs_array_type->data_type,
                          lhs_array_type->data_type, from_val, to_ptr);
      }
    } else {
      NOT_YET;
    }

  } else {
    auto fn = GetFuncReferencedIn(scope, "__assign__",
                                  Func(Tup({Ptr(lhs_type), rhs_type}), Void));
    assert(fn);
    IR::Call(Void, IR::Value(fn), {to_var, from_val});
  }
}

void Type::CallAssignment(Scope *scope, Type *lhs_type, Type *rhs_type,
                          llvm::Value *lhs_ptr, llvm::Value *rhs) {
  assert(scope);
  auto assign_fn = GetFunctionReferencedIn(scope, "__assign__",
                                           Func(Tup({Ptr(lhs_type), rhs_type}), Void));
  if (assign_fn) {
    builder.CreateCall(assign_fn, {lhs_ptr, rhs});
  } else if (lhs_type->is_primitive() || lhs_type->is_pointer() ||
             lhs_type->is_enum()) {
    builder.CreateStore(rhs, lhs_ptr);

  } else if (lhs_type->is_struct()) {
    builder.CreateCall(((Structure *)lhs_type)->assign(), {lhs_ptr, rhs});

  } else if (lhs_type->is_array()) {
    auto assign_fn = ((Array *)lhs_type)->assign();
    assert(rhs_type->is_array());
    auto rhs_array_type = (Array *)rhs_type;

    llvm::Value *rhs_len, *rhs_ptr;
    if (rhs_array_type->fixed_length) {
      rhs_len = data::const_uint(rhs_array_type->len);
      rhs_ptr =
          builder.CreateGEP(rhs, {data::const_uint(0), data::const_uint(0)});
    } else {
      rhs_len = builder.CreateLoad(
          builder.CreateGEP(rhs, {data::const_uint(0), data::const_uint(0)}));
      rhs_ptr = builder.CreateLoad(
          builder.CreateGEP(rhs, {data::const_uint(0), data::const_uint(1)}));
    }

    builder.CreateCall(assign_fn, {lhs_ptr, rhs_len, rhs_ptr});

  } else if (lhs_type->is_function()) {
    builder.CreateStore(rhs, lhs_ptr);

  } else {
    std::cerr << "LHS type = " << *lhs_type << std::endl;
    std::cerr << "RHS type = " << *rhs_type << std::endl;
    assert(false);
  }
}

void Type::CallDestroy(Scope *scope, llvm::Value *var) {

  if (is_primitive() || is_pointer() || is_enum()) {
    return;

  } else if (is_array()) {
    auto array_type = (Array *)this;
    if (array_type->data_type->requires_uninit()) {
      builder.CreateCall(array_type->destroy(), {var});
    } else {
      if (array_type->fixed_length) {
        // TODO
      } else {
        builder.CreateCall(
            cstdlib::free(),
            builder.CreateBitCast(
                builder.CreateLoad(builder.CreateGEP(
                    var, {data::const_uint(0), data::const_uint(1)})),
                *RawPtr));
      }
    }

  } else if (is_struct()) {
    // TODO if (!requires_uninit()) { return; }

    llvm::Value *destroy_fn = nullptr;

    if (scope) {
      destroy_fn =
          GetFunctionReferencedIn(scope, "__destroy__", Func(Ptr(this), Void));
    }

    // Use default destroy if none is given.
    if (!destroy_fn) { destroy_fn = ((Structure *)this)->destroy(); }
    builder.CreateCall(destroy_fn, {var});

  } else {
    assert(false && "No destructor to call");
  }
}

Primitive::Primitive(Primitive::TypeEnum pt) : type_(pt), repr_fn_(nullptr) {
  switch (type_) {
  case Primitive::TypeEnum::Bool:
    llvm_type = llvm::Type::getInt1Ty(llvm::getGlobalContext());
    break;
  case Primitive::TypeEnum::Char:
    llvm_type = llvm::Type::getInt8Ty(llvm::getGlobalContext());
    break;
  case Primitive::TypeEnum::Int:
    llvm_type = llvm::Type::getInt64Ty(llvm::getGlobalContext());
    break;
  case Primitive::TypeEnum::Real:
    llvm_type = llvm::Type::getDoubleTy(llvm::getGlobalContext());
    break;
  case Primitive::TypeEnum::Uint:
    llvm_type = llvm::Type::getInt64Ty(llvm::getGlobalContext());
    break;
  case Primitive::TypeEnum::Void:
    llvm_type = llvm::Type::getVoidTy(llvm::getGlobalContext());
    break;
  default: llvm_type = nullptr;
  }
}

Array::Array(Type *t)
    : init_fn_(nullptr), destroy_fn_(nullptr), repr_fn_(nullptr),
      assign_fn_(nullptr), init_func(nullptr), repr_func(nullptr), data_type(t),
      len(0), fixed_length(false) {
  dimension =
      data_type->is_array() ? 1 + ((Array *)data_type)->dimension : 1;

  std::vector<llvm::Type *> init_args(dimension + 1, *Uint);
  init_args[0] = *Ptr(this);
  has_vars     = data_type->has_vars;
}

Array::Array(Type *t, size_t l)
    : init_fn_(nullptr), destroy_fn_(nullptr), repr_fn_(nullptr),
      assign_fn_(nullptr), init_func(nullptr), repr_func(nullptr), data_type(t),
      len(l), fixed_length(true) {
  dimension = data_type->is_array() ? 1 + ((Array *)data_type)->dimension : 1;
  if (time() != Time::compile) {
    std::vector<llvm::Type *> init_args(dimension + 1, *Uint);
    init_args[0] = *Ptr(this);
  }
  has_vars = data_type->has_vars;
}

Tuple::Tuple(const std::vector<Type *> &entries) : entries(entries) {
  for (const auto &entry : entries) {
    if (has_vars) break;
    has_vars = entry->has_vars;
  }
}

Pointer::Pointer(Type *t) : pointee(t) { has_vars = pointee->has_vars; }

Function::Function(Type *in, Type *out) : input(in), output(out) {
  has_vars = input->has_vars || output->has_vars;
}

Enumeration::Enumeration(const std::string &name,
                         const AST::EnumLiteral *enumlit)
    : bound_name(name), string_data(nullptr) {
  llvm_type = *Uint;

  llvm::IRBuilder<> bldr(llvm::getGlobalContext());

  // TODO Use bldr to create a global array of enum_size char ptrs

  auto num_members = enumlit->members.size();
  std::vector<llvm::Constant *> enum_str_elems(num_members, nullptr);

  size_t i = 0;
  for (const auto &idstr : enumlit->members) {
    int_values[idstr] = i;

    auto enum_str = new llvm::GlobalVariable(
        /*      Module = */ *global_module,
        /*        Type = */ llvm::ArrayType::get(*Char, idstr.size() + 1),
        /*  isConstant = */ true,
        /*     Linkage = */ llvm::GlobalValue::PrivateLinkage,
        /* Initializer = */ llvm::ConstantDataArray::getString(
            llvm::getGlobalContext(), idstr, true),
        /*        Name = */ idstr);
    enum_str->setAlignment(1);
    enum_str_elems[i] = llvm::ConstantExpr::getGetElementPtr(
        llvm::ArrayType::get(*Char, idstr.size() + 1), enum_str,
        llvm::ArrayRef<llvm::Constant *>{data::const_uint(0),
                                         data::const_uint(0)});

    ++i;
  }
  string_data = new llvm::GlobalVariable(
      /*      Module = */ *global_module,
      /*        Type = */ llvm::ArrayType::get(*Ptr(Char), num_members),
      /*  isConstant = */ false,
      /*     Linkage = */ llvm::GlobalValue::ExternalLinkage,
      /* Initializer = */ llvm::ConstantArray::get(
          llvm::ArrayType::get(*Ptr(Char), num_members), enum_str_elems),
      /*        Name = */ bound_name + ".name.array");
}

ParametricStructure::ParametricStructure(const std::string &name,
                                         AST::ParametricStructLiteral *expr)
    : ast_expression(expr), bound_name(name) {}

// Create a opaque struct
Structure::Structure(const std::string &name, AST::StructLiteral *expr)
    : ast_expression(expr), bound_name(name), field_offsets(1, 0),
      creator(nullptr), init_fn_(nullptr), destroy_fn_(nullptr),
      assign_fn_(nullptr), init_func(nullptr) {}

Type *Structure::field(const std::string &name) const {
  auto iter = field_name_to_num.find(name);
  return (iter == field_name_to_num.end()) ? nullptr
                                           : field_type.at(iter->second);
}

llvm::Value *Structure::field_num(const std::string &name) const {
  auto iter = field_name_to_num.find(name);
  if (iter == field_name_to_num.end()) { return nullptr; }
  auto num = iter->second;
  auto t = field_type AT(num);
  assert(!t->is_function() && t != Type_ && "Invalid data field");

  return data::const_uint(field_num_to_llvm_num.at(num));
}

size_t Enumeration::get_index(const std::string &str) const {
  auto iter = int_values.find(str);
  if (iter == int_values.end()) {
    assert(false && "Invalid enumeration value");
  } else {
    return iter->second;
  }
}

llvm::Value *Enumeration::get_value(const std::string &str) const {
  return data::const_uint(get_index(str));
}

bool Array::requires_uninit() const { return true; }
bool Structure::requires_uninit() const {
  for (const auto t : field_type) {
    if (t->requires_uninit()) return true;
  }
  return false;
}

void Structure::set_name(const std::string &name) {
  bound_name = name;
  if (name == "string") { String = this; }
}

void ParametricStructure::set_name(const std::string &name) {
  bound_name = name;
  assert(ast_expression);
  for (auto &kv : ast_expression->cache) {
    assert(kv.second->value.as_type);
    // NOTE This is pretty hacky: Find the first paren.
    // TODO better way would be to cache not just the struct but it's parameters
    // as well.
    auto &str_name   = ((Structure *)kv.second->value.as_type)->bound_name;
    size_t paren_pos = str_name.find('(');
    assert(paren_pos != std::string::npos);
    str_name =
        bound_name + str_name.substr(paren_pos, str_name.size() - paren_pos);
  }
}

std::ostream &operator<<(std::ostream &os, const Type &t) {
  return os << t.to_string();
}

Type::operator llvm::Type *() const {
  generate_llvm();
  return llvm_type;
}

Function::operator llvm::FunctionType *() const {
  generate_llvm();
  return (llvm::FunctionType *)llvm_type;
}

void Structure::insert_field(const std::string &name, Type *ty,
                             AST::Expression *init_val) {

  // TODO what if ty->alignment() == 0?
  size_t last_field_offset = field_offsets.back();
  size_t next_offset = MoveForwardToAlignment(
      last_field_offset + (field_type.empty() ? 0 : field_type.back()->bytes()),
      ty->alignment());
  field_offsets.push_back(next_offset);


  auto next_num           = field_num_to_name.size();
  field_name_to_num[name] = next_num;
  field_num_to_name.push_back(name);
  field_type.push_back(ty);

  { // Check sizes align
    size_t size1 = field_name_to_num.size();
    size_t size2 = field_num_to_name.size();
    size_t size3 = field_type.size();
    assert(size1 == size2 && size2 == size3 &&
           "Size mismatch in struct database");
  }

  if (!ty->is_function() && ty != Type_) {
    size_t next_llvm                = field_num_to_llvm_num.size();
    field_num_to_llvm_num[next_num] = next_llvm;
  }

  // By default, init_val is nullptr;
  init_values.emplace_back(init_val);

  has_vars |= ty->has_vars;
}

bool Type::is_big() const { return is_array() || is_struct() || is_function(); }
bool Type::stores_data() const {
  return this != Type_ && !is_function() && !is_type_variable();
}

std::ostream &operator<<(std::ostream &os, const Type *&t) { return os << *t; }
