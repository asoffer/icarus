#ifndef ICARUS_UNITY
#include "Type.h"
#include <iostream>
#endif

extern llvm::Module *global_module;

// This method allocates stack space for each particular type.

llvm::Value *Function::allocate() const {
  return llvm::Function::Create(static_cast<llvm::FunctionType *>(llvm_type),
                                llvm::Function::ExternalLinkage, "",
                                global_module);
}

llvm::Value *Array::allocate() const {
  auto alloc = builder.CreateAlloca(*this);
  if (!fixed_length) { alloc->setName("tmp_array"); }
  return alloc;
}

llvm::Value *Tuple::allocate() const {
  return nullptr; // TODO
}
