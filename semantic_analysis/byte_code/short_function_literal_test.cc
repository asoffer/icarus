#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "semantic_analysis/type_system.h"
#include "test/repl.h"

namespace semantic_analysis {
namespace {

TEST(FunctionLiteral, NoParameters) {
  test::Repl repl;

  IrFunction const& f =
      *repl.execute<IrFunction const*>("() => true");
  bool result;
  data_types::IntegerTable table;
  jasmin::ExecutionState<InstructionSet> state{table};

  jasmin::Execute(f, state, {}, result);
  EXPECT_TRUE(result);
}

TEST(FunctionLiteral, OneParameter) {
  test::Repl repl;

  IrFunction const& f =
      *repl.execute<IrFunction const*>("(n: i64) => -n");
  int64_t result;
  data_types::IntegerTable table;
  jasmin::ExecutionState<InstructionSet> state{table};

  jasmin::Execute(f, state, {int64_t{3}}, result);
  EXPECT_EQ(result, -3);

  jasmin::Execute(f, state, {int64_t{0}}, result);
  EXPECT_EQ(result, 0);

  jasmin::Execute(f, state, {int64_t{-5}}, result);
  EXPECT_EQ(result, 5);
}

}  // namespace
}  // namespace semantic_analysis