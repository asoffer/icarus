#ifndef ICARUS_UNITY
#include "Type.h"
#endif

#ifdef DEBUG
#define AT(access) .at( (access) )
#else
#define AT(access) [ (access) ]
#endif

extern llvm::Module* global_module;

void Array::generate_llvm() const {
  if (llvm_type) return;

  if (fixed_length) {
    llvm_type = llvm::ArrayType::get(*data_type, len);

  } else {
    data_type->generate_llvm();

    auto struct_type = llvm::StructType::create(global_module->getContext());

    struct_type->setBody({*Uint, *Ptr(data_type)}, /* isPacked = */ false);
    struct_type->setName(Mangle(this));
    llvm_type = struct_type;
  }
}

void Pointer::generate_llvm() const {
  if (llvm_type) return;
  pointee->generate_llvm();
  llvm_type = llvm::PointerType::getUnqual(pointee->llvm_type);
}

static void AddLLVMInput(Type *t, std::vector<llvm::Type *> &input_vec) {
  if (t->is_pointer()) {
    input_vec.push_back(*t);
    return;
  }

  if (t == Void || t == Type_ || t->has_vars) { return; }
  assert(t->llvm_type);

  if (t->is_primitive() || t->is_enum()) {
    input_vec.push_back(*t);

  } else if (t->is_big()) {
    input_vec.push_back(*Ptr(t));

  } else {
    std::cerr << *t << std::endl;
    assert(false && "Not yet implemented");
  }
}

void Function::generate_llvm() const {
  if (llvm_type) return;
  input->generate_llvm();
  output->generate_llvm();
  std::vector<llvm::Type *> llvm_in;
  llvm::Type *llvm_out = *Void;

  if (input->is_tuple()) {
    auto in_tup = static_cast<Tuple *>(input);
    for (auto t : in_tup->entries) { AddLLVMInput(t, llvm_in); }

  } else {
    AddLLVMInput(input, llvm_in);
  }

  if (output->is_tuple()) {
    auto out_tup = static_cast<Tuple *>(output);
    for (auto t : out_tup->entries) { AddLLVMInput(Ptr(t), llvm_in); }

  } else if (output->is_enum() || output->is_primitive()) {
    llvm_out = *output;
    if (llvm_out == nullptr) {
      llvm_type = nullptr;
      return;
    }

  } else {
    AddLLVMInput(Ptr(output), llvm_in);
  }

  llvm_type = llvm::FunctionType::get(llvm_out, llvm_in, false);
}

void Tuple::generate_llvm() const {
  if (llvm_type) return;
  for (auto t : entries) t->generate_llvm();
}

void Structure::generate_llvm() const {
  if (llvm_type) return;

  auto struct_type = llvm::StructType::create(global_module->getContext());
  llvm_type = struct_type;

  for (const auto &f : field_type) f->generate_llvm();

  struct_type->setName(bound_name);
}

void ParametricStructure::generate_llvm() const {}
void DependentType::generate_llvm() const {}
void TypeVariable::generate_llvm() const {}
void QuantumType::generate_llvm() const {}
void RangeType::generate_llvm() const {} // TODO Assert false?

void Enumeration::generate_llvm() const { /* Generated on creation */ }
void Primitive::generate_llvm() const { /* Generated on creation */ }

// TODO make this a member of Structure instead
void AST::StructLiteral::build_llvm_internals() {
  if (params.empty()) {
    assert(type_value->is_struct());
    auto tval = static_cast<Structure *>(type_value);

    auto llvm_struct_type =
        static_cast<llvm::StructType *>(tval->llvm_type);
    if (!llvm_struct_type->isOpaque()) return;
  
    for (const auto &decl : declarations) {
      if (decl->type->has_vars) return;
    }
  
    size_t num_data_fields = tval->field_num_to_llvm_num.size();
    std::vector<llvm::Type *> llvm_fields(num_data_fields, nullptr);
    for (const auto& kv : tval->field_num_to_llvm_num) {
      llvm_fields[kv.second] = tval->field_type AT(kv.first)->llvm_type;
    }

    static_cast<llvm::StructType *>(tval->llvm_type)
        ->setBody(std::move(llvm_fields), /* isPacked = */ false);
  } else {
    assert(false);
  }
}
#undef AT
