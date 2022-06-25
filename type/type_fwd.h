#ifndef ICARUS_TYPE_TYPE_FWD_H
#define ICARUS_TYPE_TYPE_FWD_H

namespace type {
template <typename T, typename InstantiationType = T>
struct LegacyGeneric;
struct LegacyType;
#define ICARUS_TYPE_TYPE_X(name) struct name;
#include "type.xmacro.h"
#undef ICARUS_TYPE_TYPE_X
}  // namespace type

#endif  // ICARUS_TYPE_TYPE_FWD_H
