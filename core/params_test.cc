#include "core/params.h"

#include "test/catch.h"

namespace core {
namespace {
constexpr bool Ambiguity(int lhs, int rhs) { return (lhs & rhs) != 0; }

TEST_CASE("creation") {
  Params<double> params;
  CHECK(params.size() == 0u);
  params.append("pi", 3.14, MUST_NOT_NAME);
  params.append("", 1234);
  CHECK(params.size() == 2u);
  CHECK(params.at(0) == Param<double>("pi", 3.14, MUST_NOT_NAME));
  CHECK(params.at(1) == Param<double>("", 1234));
}

TEST_CASE("append") {
  Params<double> params;
  CHECK(params.size() == 0u);
  params.append("pi", 3.14, MUST_NOT_NAME);
  params.append("", 1234);
}

TEST_CASE("index") {
  Params<double> params{Param<double>{"pi", 3.14}, Param<double>{"e", 2.718},
                          Param<double>{"", 1234}, Param<double>{"", 5678},
                          Param<double>{"phi", 1.618}};

  CHECK(params.size() == 5u);
  CHECK(*params.at_or_null("pi") == 0u);
  CHECK(*params.at_or_null("e") == 1u);
  CHECK(*params.at_or_null("phi") == 4u);
}

TEST_CASE("transform") {
  Params<int> int_params{Param<int>{"a", 1, MUST_NOT_NAME},
                           Param<int>{"b", 2}, Param<int>{"", 3}};
  Params<double> double_params =
      int_params.Transform([](int n) { return n * 0.5; });

  CHECK(double_params.size() == 3u);
  CHECK(double_params.at(0) == Param<double>("a", 0.5, MUST_NOT_NAME));
  CHECK(double_params.at(1) == Param<double>("b", 1));
  CHECK(double_params.at(2) == Param<double>("", 1.5));

  CHECK(*double_params.at_or_null("a") == 0u);
  CHECK(*double_params.at_or_null("b") == 1u);
}

TEST_CASE("ambiguously callable") {
  SECTION("Both empty") {
    Params<int> p1, p2;
    CHECK(AmbiguouslyCallable(p1, p2, Ambiguity));
    CHECK(AmbiguouslyCallable(p2, p1, Ambiguity));
  }

  SECTION("One empty") {
    Params<int> p1{Param<int>{"a", 1}};
    Params<int> p2;
    CHECK_FALSE(AmbiguouslyCallable(p1, p2, Ambiguity));
    CHECK_FALSE(AmbiguouslyCallable(p2, p1, Ambiguity));
  }

  SECTION("One empty but has default") {
    Params<int> p1{Param<int>{"a", 1, HAS_DEFAULT}};
    Params<int> p2;
    CHECK(AmbiguouslyCallable(p1, p2, Ambiguity));
    CHECK(AmbiguouslyCallable(p2, p1, Ambiguity));
  }

  SECTION("Same type, different names") {
    Params<int> p1{Param<int>{"a1", 1}};
    Params<int> p2{Param<int>{"a2", 1}};
    CHECK(AmbiguouslyCallable(p1, p2, Ambiguity));
    CHECK(AmbiguouslyCallable(p2, p1, Ambiguity));
  }

  SECTION("Same type, same name") {
    Params<int> p{Param<int>{"a1", 1}};
    CHECK(AmbiguouslyCallable(p, p, Ambiguity));
  }

  SECTION("Same name different types") {
    Params<int> p1{Param<int>{"a", 1}};
    Params<int> p2{Param<int>{"a", 2}};
    CHECK_FALSE(AmbiguouslyCallable(p1, p2, Ambiguity));
    CHECK_FALSE(AmbiguouslyCallable(p2, p1, Ambiguity));
  }

  SECTION("Unambiguous because a parameter would have to be named.") {
    Params<int> p1{Param<int>{"a", 1}, Param<int>{"b", 2, HAS_DEFAULT}};
    Params<int> p2{Param<int>{"b", 2}};
    CHECK_FALSE(AmbiguouslyCallable(p1, p2, Ambiguity));
    CHECK_FALSE(AmbiguouslyCallable(p2, p1, Ambiguity));
  }

  SECTION("Both defaultable of different types") {
    Params<int> p1{Param<int>{"a", 1, HAS_DEFAULT}};
    Params<int> p2{Param<int>{"b", 2, HAS_DEFAULT}};
    CHECK(AmbiguouslyCallable(p1, p2, Ambiguity));
    CHECK(AmbiguouslyCallable(p2, p1, Ambiguity));
  }

  // TODO Many many more tests covering MUST_NOT_NAME
  SECTION("Anonymous") {
    Params<int> p1{Param<int>{"", 1, MUST_NOT_NAME}};
    Params<int> p2{Param<int>{"", 2, MUST_NOT_NAME}};
    CHECK_FALSE(AmbiguouslyCallable(p1, p2, Ambiguity));
  }
}

TEST_CASE("set") {
  Params<int> p(2);

  CHECK(p.at_or_null("n") == nullptr);

  p.set(1, Param<int>("n", 3));
  CHECK(p.at_or_null("n") != nullptr);
}

}  // namespace
}  // namespace core
