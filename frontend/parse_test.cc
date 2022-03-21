#include "frontend/parse_test.h"

#include <functional>

#include "base/meta.h"
#include "diagnostic/consumer/trivial.h"
#include "frontend/lex/lex.h"
#include "frontend/lexeme.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace frontend {
namespace {
using ::testing::IsEmpty;

struct TestParameters {
  std::string content;
  bool success;
};

TestParameters Success(std::string content) {
  return {.content = std::move(content), .success = true};
}
TestParameters Failure(std::string content) {
  return {.content = std::move(content), .success = false};
}

struct BoundTestParameters {
  template <auto ParseFn>
  static BoundTestParameters Make(TestParameters p) {
    using output_parameter_type = std::decay_t<base::head<base::tail<
        typename base::Signature<decltype(ParseFn)>::parameter_type_list>>>;
    return {.parse = [](absl::Span<Lexeme const> &lexemes) -> bool {
              output_parameter_type out;
              return ParseFn(lexemes, out);
            },
            .content = std::move(p.content),
            .success = p.success};
  }

  friend std::ostream& operator<<(std::ostream& os, BoundTestParameters const&btp) {
    return os << "Expect " << (btp.success ? "success" : "failure") << ": "
              << btp.content;
  }

  std::function<bool(absl::Span<Lexeme const> &)> parse;
  std::string content;
  bool success;
};

template <auto ParseFn>
auto InputFor(auto &&... input) requires(
    (base::meta<std::decay_t<decltype(input)>> ==
     base::meta<TestParameters>)and...) {
  return testing::ValuesIn(
      {BoundTestParameters::Make<ParseFn>(std::move(input))...});
}

struct ParseTest: testing::TestWithParam<BoundTestParameters> {};
TEST_P(ParseTest, Test) {
  auto [parse, content, success] = GetParam();
  SCOPED_TRACE(content);
  diagnostic::TrivialConsumer diagnostic_consumer;
  auto lex_result = Lex(content, diagnostic_consumer);
  if (not lex_result) {
    if (success) {
      ADD_FAILURE() << "Failed to lex input.";
    } else {
      return;
    }
  }

  absl::Span<Lexeme const> lexemes = lex_result->lexemes_;
  if (success) {
    ASSERT_TRUE(parse(lexemes));
    EXPECT_THAT(lexemes, IsEmpty());
  } else {
    ASSERT_FALSE(parse(lexemes) and lexemes.empty());
  }
}

INSTANTIATE_TEST_SUITE_P(DeclarationId, ParseTest,
                         InputFor<ParseDeclarationId>(
                             Success("n"), Failure("blah blah"),
                             Success("blah"), Failure("true"),
                             Failure("module"), Failure("builtin"),
                             Failure("import"), Failure("17"), Success("(+)")));
INSTANTIATE_TEST_SUITE_P(
    Label, ParseTest,
    InputFor<ParseLabel>(Success("#.something"), Failure("#.true"),
                         Failure("blah"), Failure("#.label more"),
                         Failure(".blah"), Failure("#blah"), Failure("# .blah"),
                         Failure("#. blah"), Failure("# . blah"),
                         Failure("#{const}")));
INSTANTIATE_TEST_SUITE_P(
    StringLiteral, ParseTest,
    InputFor<ParseStringLiteral>(Success(R"("")"), Success(R"("\n")"),
                                 Success(R"("\\n")"), Success(R"("blah")"),
                                 Failure(R"("" "")"), Failure(R"("blah)"),
                                 Success(R"("\nblah")")));
INSTANTIATE_TEST_SUITE_P(
    CallArgument, ParseTest,
    InputFor<ParseCallArgument>(Success("3"), Success("3 + 4"),
                                Success("name = 3"), Success("name = 3 + 4"),
                                Failure("3 = 4"), Failure("name = name = 3"),
                                Failure("name =")));

INSTANTIATE_TEST_SUITE_P(
    YieldStatement, ParseTest,
    InputFor<ParseYieldStatement>(
        Success("<<"), Success("<< 0"), Success("<< 3 + 4"), Success("<< 3, 4"),
        Success("<< 3, name = 4"), Success("<< name = 3, other_name = 4"),
        Failure("<< 3 << 4"),

        Failure("x <<"), Failure("x << 0"), Failure("x << 3 + 4"),
        Failure("x << 3, 4"), Failure("x << 3, name = 4"),
        Failure("x << name = 3, other_name = 4"), Failure("x << 3 << 4"),

        Success("#.label <<"), Success("#.label << 0"),
        Success("#.label << 3 + 4"), Success("#.label << 3, 4"),
        Success("#.label << 3, name = 4"),
        Success("#.label << name = 3, other_name = 4"),
        Failure("#.label << 3 << 4"),

        Failure("#bad_label <<"), Failure("#bad_label << 0"),
        Failure("#bad_label << 3 + 4"), Failure("#bad_label << 3, 4"),
        Failure("#bad_label << 3, name = 4"),
        Failure("#bad_label << name = 3, other_name = 4"),
        Failure("#bad_label << 3 << 4")));

INSTANTIATE_TEST_SUITE_P(ReturnStatement, ParseTest,
                         InputFor<ParseReturnStatement>(
                             Success("return"), Success("return 0"),
                             Success("return 3 + 4"), Success("return 3, 4"),
                             Success("return 3, name = 4"),
                             Success("return name = 3, other_name = 4"),
                             Failure("return 3 return 4")));

INSTANTIATE_TEST_SUITE_P(
    Declaration, ParseTest,
    InputFor<ParseDeclaration>(
        Success("x: y"), Success("x: y + z"), Success("x: y = z"),
        Success("x: y + z = a + b"), Success("x :: y"), Success("x :: y + z"),
        Success("x :: y = z"), Success("x :: y + z = a + b"), Success("x := y"),
        Success("x := y + z"), Success("x ::= z"), Success("x ::= y + z"),
        Failure("x: y z"), Success("(x, y): z"), Success("(x, y) :: z"),
        Failure("(x + y, z) :: w"), Success("(x) :: w"), Success("(+) :: w"),
        Failure("(+, -) :: w"), Success("((+), (-)) :: w")));

}  // namespace
}  // namespace frontend