#include "type/all.h"

#include "architecture.h"
#include "ast/declaration.h"
#include "ast/struct_literal.h"
#include "base/container/map.h"
#include "base/container/unordered_map.h"
#include "base/guarded.h"
#include "context.h"
#include "ir/arguments.h"
#include "ir/components.h"
#include "ir/func.h"
#include "ir/phi.h"
#include "module.h"

namespace type {
#define PRIMITIVE_MACRO(EnumName, name)                                        \
  Type const *EnumName = new Primitive(PrimType::EnumName);
#include "type/primitive.xmacro.h"
#undef PRIMITIVE_MACRO

static base::guarded<
    base::unordered_map<Type const *, base::unordered_map<size_t, Array>>>
    fixed_arrays_;
const Array *Arr(Type const *t, size_t len) {
  auto handle = fixed_arrays_.lock();
  return &(*handle)[t]
              .emplace(std::piecewise_construct, std::forward_as_tuple(len),
                       std::forward_as_tuple(t, len))
              .first->second;
}
static base::guarded<base::unordered_map<Type const *, Array>> arrays_;
const Array *Arr(Type const *t) {
  return &arrays_.lock()
              ->emplace(std::piecewise_construct, std::forward_as_tuple(t),
                        std::forward_as_tuple(t))
              .first->second;
}

static base::guarded<base::map<base::vector<Type const *>, Variant>> variants_;
Type const *Var(base::vector<Type const *> variants) {
  if (variants.empty()) { return type::Void(); }
  if (variants.size() == 1) { return variants[0]; }

  size_t end = variants.size();
  size_t i   = 0;
  while (i < end) {
    if (variants[i]->is<Variant>()) {
      const Variant *var = &variants[i]->as<Variant>();
      variants[i]        = variants.back();
      variants.pop_back();
      variants.insert(variants.end(), var->variants_.begin(),
                      var->variants_.end());
    } else {
      ++i;
    }
  }

  // TODO This sort order should be deterministic to allow interoperability
  // between multiple runs of the compiler.

  std::sort(variants.begin(), variants.end());
  variants.erase(std::unique(variants.begin(), variants.end()), variants.end());

  if (variants.size() == 1) { return variants.front(); }

  return &variants_.lock()
              ->emplace(std::piecewise_construct,
                        std::forward_as_tuple(variants),
                        std::forward_as_tuple(variants))
              .first->second;
}

static base::guarded<base::unordered_map<Type const *, Pointer const >>
    pointers_;
Pointer const *Ptr(Type const *t) {
  return &pointers_.lock()->emplace(t, Pointer(t)).first->second;
}

static base::guarded<base::unordered_map<Type const *, BufferPointer const >>
    buffer_pointers_;
BufferPointer const *BufPtr(Type const *t) {
  return &buffer_pointers_.lock()->emplace(t, BufferPointer(t)).first->second;
}

static base::guarded<base::map<base::vector<Type const *>,
                               base::map<base::vector<Type const *>, Function>>>
    funcs_;
const Function *Func(base::vector<Type const *> in,
                     base::vector<Type const *> out) {
  // TODO if void is unit in some way we shouldn't do this.
  auto f = Function(in, out);
  return &(*funcs_.lock())[std::move(in)]
              .emplace(std::move(out), std::move(f))
              .first->second;
}

Type const *Void() { return Tup({}); }

bool Type::is_big() const {
  return is<Array>() || is<Struct>() || is<Variant>() || is<Tuple>();
}

static base::guarded<base::unordered_map<size_t, CharBuffer>> char_bufs_;
const CharBuffer *CharBuf(size_t len) {
  auto[iter, success] = char_bufs_.lock()->emplace(len, CharBuffer(len));
  return &iter->second;
}

void Struct::finalize() {
  auto *ptr = to_be_completed_.exchange(nullptr);
  if (ptr != nullptr) { ptr->Complete(this); }
}

base::vector<Struct::Field> const &Struct::fields() const {
  // TODO is doing this eazily a good idea?
  // TODO remove this const_cast
  const_cast<type::Struct *>(this)->finalize();
  return fields_;
}

void Struct::set_last_name(std::string_view s) {
  fields_.back().name = std::string(s);
  auto[iter, success] =
      field_indices_.emplace(fields_.back().name, fields_.size() - 1);
  ASSERT(success);
}
size_t Struct::index(std::string const &name) const {
  // TODO is doing this lazily a good idea?
  // TODO remove this const_cast
  const_cast<type::Struct *>(this)->finalize();
  return field_indices_.at(name);
}

Struct::Field const *Struct::field(std::string const &name) const {
  // TODO is doing this lazily a good idea?
  // TODO remove this const_cast
  const_cast<type::Struct *>(this)->finalize();

  auto iter = field_indices_.find(name);
  if (iter == field_indices_.end()) { return nullptr; }
  return &fields_[iter->second];
}

Type const *Generic = new GenericFunction;

ir::Val Tuple::PrepareArgument(Type const *t, ir::Val const &val,
                               Context *ctx) const {
  UNREACHABLE();
}

}  // namespace type
