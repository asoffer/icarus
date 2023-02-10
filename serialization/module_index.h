#ifndef ICARUS_SERIALIZATION_MODULE_INDEX_H
#define ICARUS_SERIALIZATION_MODULE_INDEX_H

#include <cstdint>

#include "base/extend.h"
#include "base/extend/absl_format.h"
#include "base/extend/absl_hash.h"

namespace serialization {

struct ModuleIndex
    : base::Extend<ModuleIndex, 1>::With<base::AbslHashExtension,
                                         base::AbslFormatExtension> {
  using underlying_type = uint32_t;

  static constexpr std::string_view kAbslFormatString = "ModuleIndex(%u)";

  constexpr ModuleIndex()
      : value_(std::numeric_limits<underlying_type>::max()) {}
  constexpr explicit ModuleIndex(underlying_type n) : value_(n) {}

  // An identifier for the module which holds all builtin data accessible
  // through the `builtin` keyword.
  static constexpr ModuleIndex Builtin() {
    return ModuleIndex(std::numeric_limits<underlying_type>::max() - 1);
  }

  // No module has this identifier.
  static constexpr ModuleIndex Invalid() { return ModuleIndex(); }

  underlying_type value() const { return value_; }

 private:
  friend base::EnableExtensions;

  underlying_type value_;
};

}  // namespace serialization

#endif  // ICARUS_SERIALIZATION_MODULE_INDEX_H
