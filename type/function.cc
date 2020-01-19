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
  auto f = Function(in, out);
  // output_span is backed by a vector that doesn't move even when the
  // containing function does so this is safe to reference even after `f` is
  // moved.
  auto output_span = f.output();
  return &(*funcs_.lock())[std::move(in)]
              .emplace(output_span, std::move(f))
              .first->second;
}

void Function::WriteTo(std::string *result) const {
  if (input.empty()) {
    result->append("()");
  } else if (input.size() == 1 and not input[0]->is<Function>()) {
    input.at(0)->WriteTo(result);
  } else {
    result->append("(");
    input.at(0)->WriteTo(result);
    for (size_t i = 1; i < input.size(); ++i) {
      result->append(", ");
      input.at(i)->WriteTo(result);
    }
    result->append(")");
  }

  result->append(" -> ");

  if (output().empty()) {
    result->append("()");
  } else if (output().size() == 1) {
    output()[0]->WriteTo(result);
  } else {
    result->append("(");
    output()[0]->WriteTo(result);
    for (size_t i = 1; i < output().size(); ++i) {
      result->append(", ");
      output()[i]->WriteTo(result);
    }
    result->append(")");
  }
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
  for (type::Type const *t : input) {
    result.append("", type::Typed<ast::Declaration const *>(nullptr, t),
                  core::MUST_NOT_NAME);
  }
  return result;
}

}  // namespace type
