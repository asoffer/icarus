#include "type.h"
#include "../ast/ast.h"

std::string Variant::to_string() const {
  std::stringstream ss;
  auto iter = variants_.begin();
  ss << (**iter).to_string();
  ++iter;
  // TODO Parentheses?
  for (; iter != variants_.end(); ++iter) {
    ss << " | " << (**iter).to_string();
  }
  return ss.str();
}

std::string Array::to_string() const {
  std::stringstream ss;
  ss << "[";
  if (fixed_length) {
    ss << len;
  } else {
    ss << "--";
  }
  Type *const *type_ptr_ptr = &data_type;

  while ((*type_ptr_ptr)->is<Array>()) {
    auto array_ptr = (const Array *)*type_ptr_ptr;
    ss << ", ";
    if (array_ptr->fixed_length) {
      ss << array_ptr->len;
    } else {
      ss << "--";
    }

    type_ptr_ptr = &array_ptr->data_type;
  }

  ss << "; " << **type_ptr_ptr << "]";
  return ss.str();
}

std::string Function::to_string() const {
  std::stringstream ss;
  if (input->is<Function>()) {
    ss << "(" << *input << ")";

  } else {
    ss << *input;
  }

  ss << " -> " << *output;
  return ss.str();
}

std::string Pointer::to_string() const {
  std::stringstream ss;

  if (pointee->is<Function>()) {
    ss << "*(" << *pointee << ")";
  } else {
    ss << "*" << *pointee;
  }
  return ss.str();
}

std::string Tuple::to_string() const {
  std::stringstream ss;

  auto iter = entries.begin();
  ss << "(" << **iter;
  ++iter;
  while (iter != entries.end()) {
    ss << ", " << **iter;
    ++iter;
  }
  ss << ")";

  return ss.str();
}

std::string RangeType::to_string() const {
  return "Range(" + end_type->to_string() + ")";
}
std::string SliceType::to_string() const {
  return array_type->to_string() + "[..]";
}

std::string Scope_Type::to_string() const {
  return "Scope(" + type_->to_string() + ")";
}
