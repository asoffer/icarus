#include "architecture.h"

size_t Architecture::alignment(const Type *t) const {
  if (t->is_primitive()) {
    switch (ptr_cast<const Primitive>(t)->type_) {
    case PrimType::Err: NOT_YET;
    case PrimType::Unknown: UNREACHABLE;
    case PrimType::Void: return 0;
    case PrimType::Bool:
    case PrimType::Char: return 1;
    case PrimType::U16: return 2;
    case PrimType::U32: return 4;
    case PrimType::Int:
    case PrimType::Uint:
    case PrimType::Real:
    case PrimType::Type:
    case PrimType::NullPtr:
    case PrimType::Code: return 8;
    case PrimType::String: return 8;
    }
  } else if (t->is_pointer()) {
    return ptr_bytes_;
  } else if (t->is_array()) {
    auto array_type = ptr_cast<const Array>(t);
    return array_type->fixed_length ? this->alignment(array_type->data_type)
                                    : ptr_align_;
  } else if (t->is_struct()) {
    auto struct_type = const_cast<Struct *>(ptr_cast<const Struct>(t));
    struct_type->CompleteDefinition();
    size_t alignment_val = 1;
    for (auto ft : struct_type->field_type) {
      auto a = this->alignment(ft);
      if (alignment_val <= a) { alignment_val = a; }
    }
    return alignment_val;
  } else if (t->is_function()) {
    return  ptr_align_;
  } else if (t->is_enum()) {
    return this->alignment(ptr_cast<const Enum>(t)->ProxyType());
  } else if (t->is_scope_type()) {
    return 1;
  } else {
    NOT_YET;
  }
  UNREACHABLE;
}

size_t Architecture::bytes(const Type *t) const {
  if (t->is_primitive()) {
    switch (ptr_cast<const Primitive>(t)->type_) {
    case PrimType::Err: NOT_YET;
    case PrimType::Unknown: UNREACHABLE;
    case PrimType::Void: return 0;
    case PrimType::Bool:
    case PrimType::Char: return 1;
    case PrimType::U16: return 2;
    case PrimType::U32: return 4;
    case PrimType::Int:
    case PrimType::Uint:
    case PrimType::Real:
    case PrimType::Type:
    case PrimType::NullPtr:
    case PrimType::Code: return 8;
    case PrimType::String: return 8;
    }
  } else if (t->is_pointer()) {
    return ptr_bytes_;
  } else if (t->is_array()) {
    auto array_type = ptr_cast<const Array>(t);
    if (array_type->fixed_length) {
      // TODO previously there was an issue where we needed to force arrays to
      // have at least one byte. This is maybe not true anymore? At the time, it
      // was because we indexed allocations by their stack location. So if an
      // array took no space it would be indexed identically to the element that
      // preceded it. This is likely no longer an issue but requires further thought.
      auto size = MoveForwardToAlignment(array_type->data_type, alignment(t)) *
                  array_type->len;
      return size ? size : 1;
    } else {
      return 16;
    }
  } else if (t->is_struct()) {
    auto struct_type = const_cast<Struct *>(ptr_cast<const Struct>(t));
    struct_type->CompleteDefinition();
    size_t num_bytes     = 0;
    for (auto ft : struct_type->field_type) {
      num_bytes += this->bytes(ft);
      num_bytes = this->MoveForwardToAlignment(ft, num_bytes);
    }

    return MoveForwardToAlignment(struct_type, num_bytes);
  } else if (t->is_function()) {
    return 2 * ptr_bytes_;
  } else if (t->is_enum()) {
    return this->bytes(ptr_cast<const Enum>(t)->ProxyType());
  } else if (t->is_scope_type()) {
    return 0;
  } else {
    NOT_YET;
  }
  UNREACHABLE;
}
