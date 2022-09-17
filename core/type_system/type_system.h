#ifndef ICARUS_CORE_TYPE_SYSTEM_TYPE_SYSTEM_H
#define ICARUS_CORE_TYPE_SYSTEM_TYPE_SYSTEM_H

#include "base/flyweight_set.h"
#include "base/meta.h"

namespace core {

// Each kind of type (e.g., builtin, pointer, etc) must use the CRTP base
// `TypeCategory` declared here and defined below.
template <typename CrtpDerived, typename StateType>
struct TypeCategory;

// A `TypeSystem` (declared here and defined below) represents a collection of
// `TypeCategory`s along with storage for the types constructed from that type
// system.
template <typename... TypeCategories>
struct TypeSystem;

// A concept matching `TypeSystem` instantiations which support the type
// category `T`.
template <typename TS, typename T>
concept TypeSystemSupporting =
    (base::meta<TS>.template is_a<TypeSystem>() and
     std::derived_from<TS, typename T::manager_type>);

// A `Type` represents a value stored in a `TypeSystem`. Two types can be
// compared for equality or have a specific `TypeCategory` extracted from them.
//
// Note that each type belongs to only a single `TypeSystem` (the type system
// used to create it). Behavior is undefined if any other `TypeSystem` is used
// to access this type (via any member function, including `is`, `get`, or
// `get_if`).
struct Type {
  // Returns `true` if and only if `*this` type was created from the type
  // category `Cat`.
  template <typename Category>
  bool is(TypeSystemSupporting<Category> auto& sys) const {
    return category_ == sys.template index<Category>();
  }

  // Returns a value of type `Category` from which `*this` was created. Behavior
  // is undefined if `*this` was not created from `Category`. This return value
  // of function is guaranteed to satisfy the constraint that:
  // `t == static_cast<Type>(t.get<Category>(sys))`
  template <typename Category>
  Category get(TypeSystemSupporting<Category> auto& sys) const {
    ASSERT(this->is<Category>() == true);
    return Category::Construct(*this, sys);
  }

  // Returns a value of type `Category` from which `*this` was created if the
  // type was created from `Category`, and `std::nullopt` otherwise.
  template <typename Category>
  std::optional<Category> get_if(
      TypeSystemSupporting<Category> auto& sys) const {
    if (not is<Category>(sys)) { return std::nullopt; }
    return get<Category>(sys);
  }

  template <typename H>
  friend H AbslHashValue(H h, Type t) {
    return H::combine(std::move(h), t.representation());
  }

  friend constexpr bool operator==(Type lhs, Type rhs) {
    return lhs.representation() == rhs.representation();
  }
  friend constexpr bool operator!=(Type lhs, Type rhs) {
    return not(lhs == rhs);
  }

 private:
  template <typename CrtpDerived, typename StateType>
  friend struct TypeCategory;

  constexpr uint64_t representation() const {
    return (value_ << uint64_t{8}) | category_;
  }

  uint64_t value_ : 64 - 8;
  uint64_t category_ : 8;
};

template <typename CrtpDerived, typename StateType>
struct TypeCategory {
  using state_type = StateType;

  // Data structure that can be used to hold and ensure uniqueness of types of
  // this category. Each `manager_type` effectively is a flyweight representing
  // a relationship between the categories `state_type` and an integer value
  // taking the place of that state.
  struct manager_type : private base::flyweight_set<state_type> {
   private:
    friend TypeCategory;

    using base_type = base::flyweight_set<state_type>;

    using base_type::from_index;
    using base_type::index;
    using base_type::insert;
  };

  explicit TypeCategory(TypeSystemSupporting<CrtpDerived> auto& m,
                        state_type const& s)
      : manager_(&m) {
    type_.category_ = m.template index<CrtpDerived>();
    type_.value_    = manager_->index(manager_->insert(s).first);
  }
  explicit TypeCategory(TypeSystemSupporting<CrtpDerived> auto& m,
                        state_type&& s)
      : manager_(&m) {
    type_.category_ = m.template index<CrtpDerived>();
    type_.value_    = manager_->index(manager_->insert(std::move(s)).first);
  }

  operator Type() const { return type_; }

  state_type const& decompose() const {
    return manager_->from_index(type_.value_);
  }

  friend bool operator==(TypeCategory const& lhs, TypeCategory const& rhs) {
    ASSERT(lhs.manager_ == rhs.manager_);
    return lhs.type_ == rhs.type_;
  }

  friend bool operator!=(TypeCategory const& lhs, TypeCategory const& rhs) {
    ASSERT(lhs.manager_ == rhs.manager_);
    return lhs.type_ != rhs.type_;
  }

 private:
  friend Type;
  static CrtpDerived Construct(Type t,
                               TypeSystemSupporting<CrtpDerived> auto& sys) {
    ASSERT(t.template is<CrtpDerived>() == true);
    static_assert(std::is_standard_layout_v<CrtpDerived>);
    static_assert(sizeof(CrtpDerived) == sizeof(TypeCategory));
    // TODO: These requirements *should* make it safe to type-pun the object
    // down to `CrtpDerived` without explicit construction, but I haven't found
    // wording that guarantees that in the standard. For now that means we're
    // going to do an extra lookup. We'll either find the wording and make this
    // faster, or add a requirement to all `CrtpDerived` class implementations
    // to provide the necessary construction API that `Type` can hook into
    // without the extra lookup.
    return CrtpDerived(sys,
                       static_cast<manager_type&>(sys).from_index(t.value_));
  }

  Type type_;
  manager_type* manager_;
};

template <typename... TypeCategories>
struct TypeSystem : TypeCategories::manager_type... {
  template <base::one_of<TypeCategories...> T>
  constexpr size_t index() const {
    size_t index = 0;
    static_cast<void>(
        ((base::meta<T> == base::meta<TypeCategories> or (++index, false)) or
         ...));
    return index;
  }
};

}  // namespace core

#endif  // ICARUS_CORE_TYPE_SYSTEM_TYPE_SYSTEM_H
