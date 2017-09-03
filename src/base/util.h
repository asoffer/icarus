#ifndef ICARUS_BASE_UTIL_H
#define ICARUS_BASE_UTIL_H

#include <type_traits>
#include <memory>

#include "debug.h"

template <typename To, typename From> To *ptr_cast(From* ptr) {
#ifdef DEBUG
  auto *result = dynamic_cast<To *>(ptr);
  ASSERT(result, "Failed to convert");
  return result;
#else
  return reinterpret_cast<To *>(ptr);
#endif
}

template <typename To, typename From> const To *ptr_cast(const From *ptr) {
#ifdef DEBUG
  const auto *result = dynamic_cast<const To *>(ptr);
  ASSERT(result, "Failed to convert");
  return result;
#else
  return reinterpret_cast<const To *>(ptr);
#endif
}

namespace base {
template <typename Base> struct Cast {
  template <typename T> bool is() const {
    static_assert(std::is_base_of<Base, T>::value,
                  "Calling is<...> but there is no inheritance relationship. "
                  "Result is vacuously false.");
    return dynamic_cast<const T *>(reinterpret_cast<const Base *>(this)) !=
           nullptr;
  }

  template <typename T> T &as() {
    static_assert(std::is_base_of<Base, T>::value,
                  "Calling as<...> but there is no inheritance relationship. "
                  "Result is vacuously false.");
    return *ptr_cast<T>(reinterpret_cast<Base *>(this));
  }

  template <typename T> const T &as() const {
    static_assert(std::is_base_of<Base, T>::value,
                  "Calling as<...> but there is no inheritance relationship. "
                  "Result is vacuously false.");
    return *ptr_cast<T>(reinterpret_cast<const Base *>(this));
  }
};

template <typename T> std::unique_ptr<T> wrap_unique(T *ptr) {
  return std::unique_ptr<T>(ptr);
}
} // namespace base

#endif // ICARUS_BASE_UTIL_H
