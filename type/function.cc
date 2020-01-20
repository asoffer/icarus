#include "type/function.h"

#include "base/guarded.h"
#include "type/typed_value.h"

namespace type {
Type const *Generic = new GenericFunction;

core::Bytes GenericFunction::bytes(core::Arch const &) const {
  return core::Host.pointer().bytes();
}

core::Alignment GenericFunction::alignment(core::Arch const &) const {
  return core::Host.pointer().alignment();
}

static base::guarded<
    std::map<std::vector<Type const *>,
             std::map<absl::Span<Type const *const>, Function>>>
    funcs_;
Function const *Func(std::vector<Type const *> in,
                     std::vector<Type const *> out) {
  // TODO if void is unit in some way we shouldn't do this.
  core::FnParams<Type const*> param_in;
  for (Type const *i : in) { param_in.append("", i); }
  auto f = Function(param_in, out);

  // output_span is backed by a vector that doesn't move even when the
  // containing function does so this is safe to reference even after `f` is
  // moved.
  auto output_span = f.output();
  return &(*funcs_.lock())[std::move(in)]
              .emplace(output_span, std::move(f))
              .first->second;
}

void Function::WriteTo(std::string *result) const {
  result->append("(");
  std::string_view sep = "";
  for (auto const &param : input()) {
    result->append(sep);
    param.value->WriteTo(result);
    sep = ", ";
  }
  result->append(") -> (");

  sep = "";
  for (Type const *out : output()) {
    result->append(sep);
    out->WriteTo(result);
    sep = ", ";
  }
  result->append(")");
}

core::Bytes Function::bytes(core::Arch const &a) const {
  return a.function().bytes();
}

core::Alignment Function::alignment(core::Arch const &a) const {
  return a.function().alignment();
}

core::FnParams<type::Typed<ast::Declaration const *>>
Function::AnonymousFnParams() const {
  core::FnParams<type::Typed<ast::Declaration const *>> result;
  for (auto const& param: input()) {
    result.append("",
                  type::Typed<ast::Declaration const *>(nullptr, param.value),
                  core::MUST_NOT_NAME);
  }
  return result;
}

}  // namespace type
