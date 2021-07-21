#include "type/slice.h"

#include <algorithm>

#include "absl/strings/str_format.h"
#include "base/global.h"

namespace type {

static base::Global<absl::node_hash_set<Slice>> cache;
Slice const *Slc(Type t) {
  ASSERT(t.valid() == true);
  return &*cache.lock()->insert(Slice(t)).first;
}

void Slice::WriteTo(std::string *result) const {
  result->append("[]");
  data_type().get()->WriteTo(result);
}

core::Bytes Slice::bytes(core::Arch const &a) const {
  return core::FwdAlign(a.pointer().bytes(), a.pointer().alignment()) +
         core::Bytes::Get<length_t>();
}

core::Alignment Slice::alignment(core::Arch const &a) const {
  return std::max(a.pointer().alignment(), core::Alignment::Get<length_t>());
}

void Slice::ShowValue(std::ostream &os,
                      ir::CompleteResultRef const &value) const {
  auto iter       = value.raw().begin();
  ir::addr_t addr = iter.read<ir::addr_t>();
  length_t length = iter.read<length_t>();
  absl::Format(&os, "slice(%p, %u)", addr, length);
}

}  // namespace type
