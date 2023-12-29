#include "parse/parser.h"

#include <stack>
#include <variant>

#include "common/resources.h"
#include "diagnostics/consumer/null.h"
#include "lexer/lexer.h"
#include "nth/meta/type.h"
#include "nth/test/test.h"
#include "parse/test/tree_node_ref.h"
#include "parse/test/matchers.h"

namespace ic {

NTH_TEST("parser/index/empty") {
  diag::NullConsumer d;
  TokenBuffer buffer = lex::Lex(R"(a[])", d);
  auto tree          = Parse(buffer, d).parse_tree;
  NTH_EXPECT(
      FromRoot(tree) >>= Module(
          ModuleStart(),
          StatementSequence(
              ScopeStart(),
              Statement(StatementStart(),
                        IndexExpression(Identifier(), IndexArgumentStart())))));
}

NTH_TEST("parser/index/one") {
  diag::NullConsumer d;
  TokenBuffer buffer = lex::Lex(R"(a[b])", d);
  auto tree          = Parse(buffer, d).parse_tree;
  NTH_EXPECT(
      FromRoot(tree) >>=
      Module(ModuleStart(),
             StatementSequence(
                 ScopeStart(),
                 Statement(StatementStart(),
                           IndexExpression(Identifier(), IndexArgumentStart(),
                                           Identifier())))));
}

NTH_TEST("parser/index/one-complex") {
  diag::NullConsumer d;
  TokenBuffer buffer = lex::Lex(R"(a[b[] + c[d]])", d);
  auto tree          = Parse(buffer, d).parse_tree;
  NTH_EXPECT(
      FromRoot(tree) >>= Module(
          ModuleStart(),
          StatementSequence(
              ScopeStart(),
              Statement(
                  StatementStart(),
                  IndexExpression(
                      Identifier(), IndexArgumentStart(),
                      ExpressionPrecedenceGroup(
                          IndexExpression(Identifier(), IndexArgumentStart()),
                          InfixOperator(),
                          IndexExpression(Identifier(), IndexArgumentStart(),
                                          Identifier())))))));
}

NTH_TEST("parser/index/precedence-with-dot") {
  diag::NullConsumer d;
  TokenBuffer buffer = lex::Lex(R"(a.b[])", d);
  auto tree          = Parse(buffer, d).parse_tree;
  NTH_EXPECT(FromRoot(tree) >>= Module(
                 ModuleStart(),
                 StatementSequence(
                     ScopeStart(),
                     Statement(StatementStart(),
                               IndexExpression(MemberExpression(Identifier()),
                                               IndexArgumentStart())))));
}

NTH_TEST("parser/index/multiple-arguments") {
  diag::NullConsumer d;
  TokenBuffer buffer = lex::Lex(R"(a.b[c, d, e])", d);
  auto tree          = Parse(buffer, d).parse_tree;
  NTH_EXPECT(
      FromRoot(tree) >>=
      Module(ModuleStart(),
             StatementSequence(
                 ScopeStart(),
                 Statement(StatementStart(),
                           IndexExpression(MemberExpression(Identifier()),
                                           IndexArgumentStart(), Identifier(),
                                           Identifier(), Identifier())))));
}

}  // namespace ic
