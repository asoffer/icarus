#ifndef ICARUS_PROPERTY_PROPERTY_MAP_H
#define ICARUS_PROPERTY_PROPERTY_MAP_H

#include "base/container/unordered_map.h"
#include "base/container/vector.h"

#include "base/owned_ptr.h"
#include "base/stale_set.h"
#include "base/util.h"
#include "ir/val.h"

namespace IR {
struct BasicBlock;
struct Func;
}  // namespace IR

namespace prop {
struct Property : public base::Cast<Property> {
  virtual ~Property() {}
  virtual Property *Clone() const = 0;
  virtual std::string to_string() const = 0;
};

template <typename T>
struct DefaultProperty : public Property {
  DefaultProperty<T>* Clone() const override {}
  std::string to_string() const override = 0;
};

template <>
struct DefaultProperty<bool> : public Property {
  DefaultProperty() : can_be_true_(true), can_be_false_(true) {}
  DefaultProperty(bool b) : can_be_true_(b), can_be_false_(!b) {}
  ~DefaultProperty() override {}

  std::string to_string() const override {
    return std::string{} + (can_be_true_ ? "y" : "n") +
           (can_be_false_ ? "y" : "n");
  }

  DefaultProperty<bool> *Clone() const override {
    auto *result          = new DefaultProperty;
    result->can_be_true_  = can_be_true_;
    result->can_be_false_ = can_be_false_;
    return result;
  }

  static DefaultProperty<bool> Bottom() {
    DefaultProperty<bool> val;
    val.can_be_true_  = false;
    val.can_be_false_ = false;
    return val;
  }

  void Merge(const DefaultProperty &p) {
    can_be_true_ |= p.can_be_true_;
    can_be_false_ |= p.can_be_false_;
  }

  bool can_be_true_  = true;
  bool can_be_false_ = true;
};

struct PropertySet {
  template <typename Prop>
  bool add(std::unique_ptr<Prop> prop) {
    props_.emplace_back(std::move(prop));
    return true;
  }

  base::vector<base::owned_ptr<Property>> props_;
};
inline std::ostream &operator<<(std::ostream &os, const PropertySet &props) {
  return os << base::internal::stringify(props.props_);
}

struct FnStateView {
  FnStateView(IR::Func *fn);

  base::unordered_map<IR::Register, PropertySet> view_;
};

inline std::ostream &operator<<(std::ostream &os, const FnStateView &fsv) {
  return os << base::internal::stringify(fsv.view_);
}

struct Entry {
  Entry(const IR::BasicBlock *viewing_block, IR::Register reg)
      : viewing_block_(viewing_block), reg_(reg) {}

  const IR::BasicBlock *viewing_block_;
  IR::Register reg_;
};

inline bool operator==(const Entry &lhs, const Entry &rhs) {
  return lhs.viewing_block_ == rhs.viewing_block_ && lhs.reg_ == rhs.reg_;
}
}  // namespace prop

namespace std {
template <>
struct hash<prop::Entry> {
  size_t operator()(const prop::Entry &e) const {
    size_t seed = 0x9e3779b97f681513ull;
    base::hash_combine(seed, e.viewing_block_);
    base::hash_combine(seed, e.reg_);
    return seed;
  }
};
}  // namespace std

namespace prop {
struct PropertyMap {
  PropertyMap() = default;
  PropertyMap(IR::Func *fn);
  PropertyMap(const PropertyMap &p) = default;
  PropertyMap &operator=(const PropertyMap &p) = default;
  PropertyMap(PropertyMap &&p) noexcept        = default;
  PropertyMap &operator=(PropertyMap &&p) noexcept = default;

  // Make a copy of this map and set the arguments to the values passed in
  PropertyMap with_args(const base::vector<IR::Val>& args) const;

  // TODO rename or delete me.
  DefaultProperty<bool> Returns() const;

  void refresh();

  IR::Func *fn_ = nullptr;
  // TODO given that you want the invariant that this is always empty...
  // probably shouldn't be storing it.
  base::stale_set<Entry> stale_entries_;
  base::unordered_map<const IR::BasicBlock *, FnStateView> view_;
};
}  // namespace prop

#endif  // ICARUS_PROPERTY_PROPERTY_MAP_H
