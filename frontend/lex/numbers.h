#ifndef ICARUS_FRONTEND_LEX_NUMBERS_H
#define ICARUS_FRONTEND_LEX_NUMBERS_H

#include <string_view>
#include <variant>

#include "ir/value/integer.h"

namespace frontend {

// This is the maximum byte-width allowed in integer literals.
constexpr inline int8_t kMaxIntBytes = 8;

enum class NumberParsingError {
  kUnknownBase,
  kTooManyDots,
  kNoDigits,
  kInvalidDigit,
  kTooLarge,
};

std::variant<ir::Integer, double, NumberParsingError> ParseNumber(
    std::string_view sv);

}  // namespace frontend

#endif  // ICARUS_FRONTEND_LEX_NUMBERS_H
