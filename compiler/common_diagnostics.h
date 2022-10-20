#ifndef ICARUS_COMPILER_COMMON_DIAGNOSTICS_H
#define ICARUS_COMPILER_COMMON_DIAGNOSTICS_H

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "diagnostic/message.h"

namespace compiler {

struct UndeclaredIdentifier {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "undeclared-identifier";

  diagnostic::DiagnosticMessage ToMessage() const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text("Found an undeclared identifier '%s':", id),
        diagnostic::SourceQuote().Highlighted(view,
                                              diagnostic::Style::ErrorText()));
  }

  std::string_view id;
  std::string_view view;
};

struct NotAType {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "not-a-type";

  diagnostic::DiagnosticMessage ToMessage() const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text("Expression was expected to be a type, but instead "
                         "was a value of type `%s`.",
                         type),
        diagnostic::SourceQuote().Highlighted(view, diagnostic::Style{}));
  }

  std::string_view view;
  std::string type;
};

struct InvalidCast {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "invalid-cast";

  diagnostic::DiagnosticMessage ToMessage() const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text("No viable cast from `%s` to `%s`.", from, to),
        diagnostic::SourceQuote().Highlighted(view, diagnostic::Style{}));
  }

  std::string from;
  std::string to;
  std::string_view view;
};

struct AssigningToConstant {
  // TODO I'm not sure this shouldn't be in the type-error category.
  static constexpr std::string_view kCategory = "value-category-error";
  static constexpr std::string_view kName     = "assigning-to-constant";

  diagnostic::DiagnosticMessage ToMessage() const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text("Cannot assign to a constant (of type `%s`).", to),
        diagnostic::SourceQuote().Highlighted(view, diagnostic::Style{}));
  }

  std::string to;
  std::string_view view;
};

struct ImmovableType {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "immovable-type";

  diagnostic::DiagnosticMessage ToMessage() const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text("Attempting to move an immovable type `%s`.", from),
        diagnostic::SourceQuote().Highlighted(view, diagnostic::Style{}));
  }

  std::string from;
  std::string_view view;
};

struct PatternTypeMismatch {
  static constexpr std::string_view kCategory = "pattern-error";
  static constexpr std::string_view kName     = "pattern-type-mismatch";

  diagnostic::DiagnosticMessage ToMessage() const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text(
            R"(Mismatched type between pattern and expression being matched.
  Type from pattern:          %s
  Type being matched against: %s)",
            pattern_type, matched_type),
        diagnostic::SourceQuote().Highlighted(view,
                                              diagnostic::Style::ErrorText()));
  }

  std::string pattern_type;
  std::string matched_type;
  std::string_view view;
};

}  // namespace compiler

#endif  // ICARUS_COMPILER_COMMON_DIAGNOSTICS_H
