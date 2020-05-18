#ifndef ICARUS_INTERPRETTER_EVALUATION_FAILURE_H
#define ICARUS_INTERPRETTER_EVALUATION_FAILURE_H

namespace interpretter {

struct EvaluationFailure {
  enum class Reason {
    NonConstant,
    Timeout,
    Unknown,
  } reason;
};

}  // namespace interpretter

#endif  // ICARUS_INTERPRETTER_EVALUATION_FAILURE_H
