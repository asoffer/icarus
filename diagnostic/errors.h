#ifndef ICARUS_DIAGNOSTIC_ERRORS_H
#define ICARUS_DIAGNOSTIC_ERRORS_H

#include <experimental/source_location>
#include <string_view>
#include <vector>

#include "absl/strings/str_cat.h"
#include "core/fn_args.h"
#include "diagnostic/message.h"
#include "frontend/lex/lex.h"
#include "frontend/lex/numbers.h"
#include "frontend/source/file_name.h"
#include "frontend/source/range.h"
#include "frontend/source/source.h"
#include "type/qual_type.h"

namespace diagnostic {

struct ArithmeticBinaryOperatorTypeMismatch {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName =
      "arithmetic-binary-operator-type-mismatch";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Mismatched types `%s` and `%s` in binary operator.",
             lhs_type->to_string(), rhs_type->to_string()),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  type::Type const *lhs_type;
  type::Type const *rhs_type;
  frontend::SourceRange range;
};

struct NonConstantTypeMemberAccess {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName = "non-constant-type-member-access";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Cannot access a member of a non-constant type."),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct InvalidCast {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "invalid-cast";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text("No viable cast form `%s` to `%s`.",
                                  from->to_string(), to->to_string()),
                             SourceQuote(src).Highlighted(range, Style{}));
  }

  type::Type const *from;
  type::Type const *to;
  frontend::SourceRange range;
};

struct ImmovableType {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "immovable-type";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Attempting to move an immovable type `%s`.", from->to_string()),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  type::Type const *from;
  frontend::SourceRange range;
};

struct MismatchedAssignmentCount {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "mismatched-assignment-count";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Assigning multiple values but left-hand and right-hand side have "
             "different numbers of elements (`%d` vs. `%d`).",
             to, from),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  size_t to;
  size_t from;
  frontend::SourceRange range;
};

struct MismatchedInitializationCount {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName = "mismatched-initialization-count";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Initializing multiple values but left-hand and right-hand side "
             "have different numbers of elements (`%d` vs. `%d`).",
             to, from),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  size_t to;
  size_t from;
  frontend::SourceRange range;
};

struct AssigningToConstant {
  // TODO I'm not sure this shouldn't be in the type-error category.
  static constexpr std::string_view kCategory = "value-category-error";
  static constexpr std::string_view kName     = "assigning-to-constant";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("It is not allowed to assign to a constant expression. In this "
             "case, the left-hand side of the assignment has type `%s`",
             to->to_string()),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  type::Type const *to;
  frontend::SourceRange range;
};

struct NumberParsingFailure {
  static constexpr std::string_view kCategory = "lex";
  static constexpr std::string_view kName     = "number-parsing-failure";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    std::string_view message;
    switch (error) {
      case frontend::NumberParsingError::kUnknownBase:
        message = "Unknown base for numeric literal";
        break;
      case frontend::NumberParsingError::kTooManyDots:
        message = "Too many `.` characters in numeric literal";
        break;
      case frontend::NumberParsingError::kNoDigits:
        message = "No digits in numeric literal";
        break;
      case frontend::NumberParsingError::kInvalidDigit:
        message = "Invalid digit encountered";
        break;
      case frontend::NumberParsingError::kTooLarge:
        message = "Numeric literal is too large";
        break;
    }

    return DiagnosticMessage(Text("%s", message),
                             SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::NumberParsingError error;
  frontend::SourceRange range;
};

struct UnprintableSourceCharacter {
  static constexpr std::string_view kCategory = "lex";
  static constexpr std::string_view kName     = "unprintable-source-character";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Uncountered unprintable character with integral value '%d' "
             "encountered in source.",
             value),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  int value;
  frontend::SourceRange range;
};

struct InvalidSourceCharacter {
  static constexpr std::string_view kCategory = "lex";
  static constexpr std::string_view kName     = "invalid-source-character";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Invalid character '%c' encountered in source.", value),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  char value;
  frontend::SourceRange range;
};

struct StringLiteralParsingFailure {
  static constexpr std::string_view kCategory = "lex";
  static constexpr std::string_view kName = "string-literal-parsing-failure";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    // TODO: Implement
    return DiagnosticMessage();
  }

  std::vector<frontend::StringLiteralError> errors;
  frontend::SourceRange range;
};

struct HashtagParsingFailure {
  static constexpr std::string_view kCategory = "lex";
  static constexpr std::string_view kName     = "hashtag-parsing-failure";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    // TODO: Implement
    return DiagnosticMessage();
  }

  frontend::SourceRange range;
};

struct NonWhitespaceAfterNewlineEscape {
  static constexpr std::string_view kCategory = "lex";
  static constexpr std::string_view kName =
      "non-whitespace-after-newline-escape";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    // TODO: Implement
    return DiagnosticMessage();
  }

  frontend::SourceRange range;
};

struct EmptySource {
  static constexpr std::string_view kCategory = "parse-error";
  static constexpr std::string_view kName     = "empty-source";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Source file is empty or contains only whitespace."));
  }
  // TODO source file identifier?
};

struct CommaSeparatedListStatement {
  static constexpr std::string_view kCategory = "parse-error";
  static constexpr std::string_view kName = "comma-separated-list-statement";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Comma-separated lists are not allowed as statements"),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct DeclarationUsedInUnaryOperator {
  static constexpr std::string_view kCategory = "parse-error";
  static constexpr std::string_view kName =
      "declaration-used-in-unary-operator";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Declarations cannot be used as argument to unary operator."),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct PositionalArgumentFollowingNamed {
  static constexpr std::string_view kCategory = "parse-error";
  static constexpr std::string_view kName =
      "positional-argument-followed-by-named";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    SourceQuote quote(src);
    quote.Highlighted(last_named, Style{});
    for (auto const &pos_range : pos_ranges) {
      quote.Highlighted(pos_range, Style{});
    }
    return DiagnosticMessage(
        Text("Positional function arguments cannot follow a named argument."),
        std::move(quote));
  }

  std::vector<frontend::SourceRange> pos_ranges;
  frontend::SourceRange last_named;
};

struct AccessRhsNotIdentifier {
  static constexpr std::string_view kCategory = "parse-error";
  static constexpr std::string_view kName     = "access-rhs-not-identifier";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text("Right-hand side must be an identifier"),
                             SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct ReservedKeyword {
  static constexpr std::string_view kCategory = "parse-error";
  static constexpr std::string_view kName     = "reserved-keyword";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Identifier `%s` is a reserved keyword.", keyword),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
  std::string keyword;
};

struct CallingDeclaration {
  static constexpr std::string_view kCategory = "parse-error";
  static constexpr std::string_view kName     = "calling-declaration";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text("Declarations cannot be called"),
                             SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct IndexingDeclaration {
  static constexpr std::string_view kCategory = "parse-error";
  static constexpr std::string_view kName     = "indexing-declaration";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text("Declarations cannot be indexed"),
                             SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct NonDeclarationInStruct {
  static constexpr std::string_view kCategory = "parse-error";
  static constexpr std::string_view kName     = "non-declaration-in-struct";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Each struct member must be defined using a declaration."),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct UnknownParseError {
  static constexpr std::string_view kCategory = "parse-error";
  static constexpr std::string_view kName     = "unknown-parse-error";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    SourceQuote quote(src);
    for (auto const &range : lines) { quote.Highlighted(range, Style{}); }
    return DiagnosticMessage(
        Text("Parse errors found in \"<SOME FILE>\" on the following lines:"),
        std::move(quote));
  }

  std::vector<frontend::SourceRange> lines;
};

struct NoReturnTypes {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "no-return-type";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Attempting to return a value when function returns nothing."),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct TypeHasNoMembers {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "type-has-no-members";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text("Cannot access a member of `type`."),
                             SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct NonConstantModuleMemberAccess {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName = "non-constant-module-member-access";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Cannot access a member of a non-constant module."),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct NoExportedSymbol {
  static constexpr std::string_view kCategory = "build-error";
  static constexpr std::string_view kName     = "no-exported-symbol";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("No exported symbol of given name in this module."),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct InconsistentArrayType {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "inconsistent-array-type";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Type error: Array literal must have consistent type"),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct NonIntegralArrayLength {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "non-integral-array-length";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text("Array length indexed by non-integral type"),
                             SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct ArrayDataTypeNotAType {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "array-data-type-not-a-type";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Array type has underlying data type specified as a value which "
             "is not a type."),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct XorEqNeedsBoolOrFlags {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "xor-needs-bool-or-flags";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Operator '^=' must take boolean or flags arguments."),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct OrEqNeedsBoolOrFlags {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "or-needs-bool-or-flags";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Operator '|=' must take boolean or flags arguments."),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct AndEqNeedsBoolOrFlags {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "and-needs-bool-or-flags";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Operator '&=' must take boolean or flags arguments."),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct NonTypeFunctionInput {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "non-type-function-output";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("The specified input type for a function must be a type."),
        SourceQuote(src).Highlighted(range, Style{}));
  }
  frontend::SourceRange range;
};

struct NonTypeFunctionOutput {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "non-type-function-output";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("The specified return type for a function must be a type."),
        SourceQuote(src).Highlighted(range, Style{}));
  }
  frontend::SourceRange range;
};

struct BuiltinError {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "builtin-error";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text("%s", message),
                             SourceQuote(src).Highlighted(range, Style{}));
  }
  frontend::SourceRange range;
  std::string message;
};

struct MissingMember {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "missing-member";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Expressions of type `%s` have no member named `%s`.",
             type->to_string(), member),
        SourceQuote(src).Highlighted(range, Style{}));
  }
  frontend::SourceRange range;
  std::string member;
  type::Type const *type;
};

struct ReturnTypeMismatch {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "return-type-mismatch";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Returning an expression of type `%s` from a function which "
             "returns `%s`.",
             actual->to_string(), expected->to_string()),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  type::Type const *actual;
  type::Type const *expected;
  frontend::SourceRange range;
};

struct ReturningWrongNumber {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "returning-wrong-number";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Attempting to return %u values from a function which has %u "
             "return values.",
             actual, expected),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  size_t actual;
  size_t expected;
  frontend::SourceRange range;
};

struct IndexedReturnTypeMismatch {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "indexed-return-type-mismatch";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Returning an expression in slot #%u (zero-indexed) of type `%s` "
             "but "
             "function expects a value of type `%s` in that slot.",
             index, actual->to_string(), expected->to_string()),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  size_t index;
  type::Type const *actual;
  type::Type const *expected;
  frontend::SourceRange range;
};

struct NonExportedMember {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "non-exported-member";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Expressions of type `%s` do not export the member `%s`.",
             type->to_string(), member),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  std::string member;
  type::Type const *type;
  frontend::SourceRange range;
};

struct CastToNonType {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "cast-to-non-type";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text("Cannot cast to a non-type."),
                             SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct CastToNonConstantType {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "cast-to-non-constant-type";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Cannot cast to a type which is not declared constant."),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct NotAType {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "not-a-type";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text("Expression was expected to be a type, but "
                                  "instead was of type `%s`.",
                                  type->to_string()),
                             SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
  type::Type const *type;
};

struct ComparingIncomparables {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "comparing-incomparables";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Values of type `%s` and `%s` are being compared but no such "
             "comparison is allowed:",
             lhs->to_string(), rhs->to_string()),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  type::Type const *lhs;
  type::Type const *rhs;
  frontend::SourceRange range;
};

struct NoDefaultValue {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "no-default-value";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("There is no default value for the type `%s`.", type->to_string()),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  type::Type const *type;
  frontend::SourceRange range;
};

struct UninferrableType {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "uninferrable-type";

  enum class Reason {
    kInferrable,
    kHole,
    kEmptyArray,
    kNullPtr,
  };

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    char const *text = nullptr;
    switch (reason) {
      case Reason::kInferrable: UNREACHABLE();
      case Reason::kEmptyArray:
        text =
            "Unable to infer the type of the following expression because the "
            "type of an empty array cannot be inferred. Either specify the "
            "type explicitly, or cast it to a specific array type:";
        break;
      case Reason::kNullPtr:
        text =
            "Unable to infer the type of the following expression because the "
            "type of `null` cannot be inferred. Either specify the type "
            "explicitly, or cast it to a specific pointer type:";
        break;
      case Reason::kHole:
        text = "Unable to infer the type of a value that is uninitalized:";
        break;
    }

    return DiagnosticMessage(Text(text),
                             SourceQuote(src).Highlighted(range, Style{}));
  }

  Reason reason;
  frontend::SourceRange range;
};

struct UninitializedConstant {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "uninitialized-constant";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Attempting to define a constant with an uninitialized value."),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct ShadowingDeclaration {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "shadowing-declaration";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text("Ambiguous declarations:"),
                             SourceQuote(src)
                                 .Line(range1.begin().line_num)
                                 .Line(range2.begin().line_num));
  }

  frontend::SourceRange range1;
  frontend::SourceRange range2;
};

struct CyclicDependency {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "cyclic-dependency";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text("Found a cyclic dependency:"));
    // TODO source quote
  }

  std::vector<std::pair<frontend::SourceRange, std::string_view>> cycle;
};

struct UndeclaredIdentifier {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "cyclic-dependency";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Found an undeclared identifier:"),
        SourceQuote(src).Highlighted(range, Style::ErrorText()));
  }

  std::string_view id;
  frontend::SourceRange range;
};

struct UnspecifiedOverload {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "unspecified-overload";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Attempting to access an overloaded function by name."),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct DeclOutOfOrder {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "declaration-out-of-order";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Variable `%s` used before it was declared.", id),
        SourceQuote(src)
            .Highlighted(use_range, Style::ErrorText())
            .Highlighted(id_range, Style::ErrorText()));
  }

  std::string_view id;
  frontend::SourceRange id_range;
  frontend::SourceRange use_range;
};

struct InvalidImport {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "invalid-import";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text("Cannot import a non-constant module."),
                             SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct NonConstantImport {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "non-constant-import";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text("Scope names must be constant."),
                             SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct MissingModule {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "missing-module";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text(
        "Could not find module named \"%s\" requested from %s", source.name(),
        requestor.empty() ? "command line"
                          : absl::StrCat("\"", requestor, "\".")));
  }

  frontend::CanonicalFileName source;
  std::string requestor;
};

struct InvalidIndexType {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "invalid-index-type";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text(
            "Attempting to index a value of type `%s` with a non-integral "
            "index. "
            "Indices must be integers, but you provided an index of type `%s`.",
            type->to_string(), index_type->to_string()),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
  type::Type const *type;
  type::Type const *index_type;
};

struct NonConstantTupleIndex {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "non-constant-tuple-index";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Index into a tuple must be a compile-time constant."),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct IndexingTupleOutOfBounds {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "indexing-tuple-out-of-bounds";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Tuple is indexed out of bounds. Tuple of type `%s` has size %u "
             "but you are attempting to access position %d.",
             tuple->to_string(), tuple->entries_.size(), index),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
  type::Tuple const *tuple;
  int64_t index;
};

struct InvalidIndexing {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "invalid-indexing";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Cannot index into a non-array, non-buffer type. Indexed type is "
             "a `%s`.",
             type->to_string()),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
  type::Type const *type;
};

struct SwitchConditionNeedsBool {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "switch-condition-needs-bool";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Expressionless switch conditions must evaluate to a `bool`, but "
             "you provided a `%s`.",
             type->to_string()),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  type::Type const *type;
  frontend::SourceRange range;
};

struct PreconditionNeedsBool {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "pre-condition-needs-bool";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Function precondition must be of type bool, but you provided an "
             "expression of type %s.",
             type->to_string()),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  type::Type const *type;
  frontend::SourceRange range;
};

struct PostconditionNeedsBool {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "postcondition-needs-bool";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Function postcondition must be of type bool, but you provided an "
             "expression of type %s.",
             type->to_string()),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  type::Type const *type;
  frontend::SourceRange range;
};

struct NonConstantEvaluation {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "non-constant-evaluation";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Cannot evaluate a non-constant at compile-time."),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct DereferencingNonPointer {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "dereferencing-non-pointer";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Attempting to dereference an object of type `%s` which is not a "
             "pointer",
             type->to_string()),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  type::Type const *type;
  frontend::SourceRange range;
};

struct WhichNonVariant {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "which-non-variant";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Attempting to call `which` an object of type `%s` which is not a "
             "variant.",
             type->to_string()),
        SourceQuote(src).Highlighted(range, Style{}));
  }

  type::Type const *type;
  frontend::SourceRange range;
};

struct ParametersDoNotCoverArguments {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName = "parameters-do-not-cover-arguments";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    // TODO
    return DiagnosticMessage(Text("Parameters do not cover arguments."));
  }

  core::FnArgs<type::QualType> const &args;
};

// TODO do we want to talk about this as already having a label, or giving it
// multiple labels? "multiple labels" seems clearer because it doesn't refer to
// parser state.
struct ScopeNodeAlreadyHasLabel {
  static constexpr std::string_view kCategory = "parse-error";
  static constexpr std::string_view kName     = "scope-already-has-label";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text("This scope already has a label."),
                             SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange label_range;
  frontend::SourceRange range;
};

struct UnknownBuiltinHashtag {
  static constexpr std::string_view kCategory = "parse-error";
  static constexpr std::string_view kName     = "unknown-builtin-hashtag";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text("Unknown builtin hashtag #%s", token),
                             SourceQuote(src).Highlighted(range, Style{}));
  }

  std::string token;
  frontend::SourceRange range;
};

struct NonAddressableExpression {
  static constexpr std::string_view kCategory = "value-category-error";
  static constexpr std::string_view kName     = "non-addressable-expression";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text("Expression is not addresable."),
                             SourceQuote(src).Highlighted(range, Style{}));
  }

  frontend::SourceRange range;
};

struct NonTypeScopeState {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "non-type-scope-state";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text("TODO"),
                             SourceQuote(src).Highlighted(range, Style{}));
  }

  type::Type const *type;
  frontend::SourceRange range;
};

struct UncallableExpression {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "uncallable-expression";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(
        Text("Uncallable expression"),
        SourceQuote(src).Highlighted(range, Style::ErrorText()));
  }

  frontend::SourceRange range;
};

struct Todo {
  static constexpr std::string_view kCategory = "todo";
  static constexpr std::string_view kName     = "todo";

  DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return DiagnosticMessage(Text("TODO: Diagnostic emit from %s, line %u.",
                                  loc.file_name(), loc.line()));
  }

  std::experimental::source_location loc =
      std::experimental::source_location::current();
};

}  // namespace diagnostic

#endif  // ICARUS_DIAGNOSTIC_ERRORS_H
