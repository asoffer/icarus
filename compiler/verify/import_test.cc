#include "compiler/compiler.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/module.h"

namespace compiler {
namespace {

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

TEST(Import, InvalidTypeBeingImported) {
  test::TestModule mod;
  EXPECT_CALL(mod.importer, Import).Times(0);
  auto const *import = mod.Append<ast::Expression>(R"(import 3)");
  auto const *qt     = mod.data().qual_type(import);
  ASSERT_NE(qt, nullptr);
  EXPECT_EQ(*qt, type::QualType::Constant(type::Module));
  EXPECT_THAT(mod.consumer.diagnostics(),
              UnorderedElementsAre(Pair("type-error", "invalid-import")));
}

TEST(Import, NonConstantImport) {
  test::TestModule mod;
  EXPECT_CALL(mod.importer, Import).Times(0);
  mod.AppendCode(R"(str := "abc")");
  auto const *import = mod.Append<ast::Expression>(R"(import str)");
  auto const *qt     = mod.data().qual_type(import);
  ASSERT_NE(qt, nullptr);
  EXPECT_EQ(*qt, type::QualType::Constant(type::Module));
  EXPECT_THAT(mod.consumer.diagnostics(),
              UnorderedElementsAre(
                  Pair("value-category-error", "non-constant-import")));
}

TEST(Import, NonConstantAndInvalidType) {
  test::TestModule mod;
  EXPECT_CALL(mod.importer, Import).Times(0);
  mod.AppendCode(R"(x := 3)");
  auto const *import = mod.Append<ast::Expression>(R"(import x)");
  auto const *qt     = mod.data().qual_type(import);
  ASSERT_NE(qt, nullptr);
  EXPECT_EQ(*qt, type::QualType::Constant(type::Module));
  EXPECT_THAT(mod.consumer.diagnostics(),
              UnorderedElementsAre(
                  Pair("type-error", "invalid-import"),
                  Pair("value-category-error", "non-constant-import")));
}

TEST(Import, ByteViewLiteral) {
  test::TestModule mod;
  EXPECT_CALL(mod.importer, Import(frontend::CanonicalFileName::Make(
                                frontend::FileName("some-module"))))
      .WillOnce(Return(ir::ModuleId(7)));
  auto const *import = mod.Append<ast::Expression>(R"(import "some-module")");
  auto const *qt     = mod.data().qual_type(import);
  ASSERT_NE(qt, nullptr);
  EXPECT_EQ(*qt, type::QualType::Constant(type::Module));
  EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
}

TEST(Import, ConstantByteView) {
  test::TestModule mod;
  EXPECT_CALL(mod.importer, Import(frontend::CanonicalFileName::Make(
                                frontend::FileName("some-module"))))
      .WillOnce(Return(ir::ModuleId(7)));
  mod.AppendCode(R"(str ::= "some-module")");
  auto const *import = mod.Append<ast::Expression>(R"(import str)");
  auto const *qt     = mod.data().qual_type(import);
  ASSERT_NE(qt, nullptr);
  EXPECT_EQ(*qt, type::QualType::Constant(type::Module));
  EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
}

}  // namespace
}  // namespace compiler