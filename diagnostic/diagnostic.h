#ifndef ICARUS_DIAGNOSTIC_DIAGNOSTIC_H
#define ICARUS_DIAGNOSTIC_DIAGNOSTIC_H

#include <string>
#include <variant>

#include "absl/strings/str_format.h"
#include "frontend/source/range.h"
#include "frontend/source/source.h"

namespace diagnostic {
// TODO Do you need `Fatal`?
enum class Category { Note, Warning, Error, Fatal };

struct Style {};

struct Highlight {
  Highlight(frontend::SourceRange range, Style style)
      : range(range), style(style) {}
  frontend::SourceRange range;
  Style style;
};

struct SourceQuote {
  SourceQuote() = delete;
  SourceQuote(frontend::Source* source) : source(source) {}

  // TODO implement for real.
  SourceQuote& Highlighted(frontend::SourceRange range, Style style) {
    lines.insert(range.lines().expanded(1).clamped_below(frontend::LineNum(1)));
    highlights.emplace_back(range, style);
    return *this;
  }

  frontend::Source* source;
  base::IntervalSet<frontend::LineNum> lines;
  std::vector<Highlight> highlights;
};

struct Text {
  explicit Text(std::string message) : message_(std::move(message)) {}
  template <typename... Args, std::enable_if_t<sizeof...(Args) != 0>* = nullptr>
  explicit Text(absl::FormatSpec<Args...> const& format, Args const&... args)
      : message_(absl::StrFormat(format, args...)) {}

  char const* c_str() const { return message_.c_str(); }

 private:
  std::string message_;
};

struct Diagnostic {
 private:
  using Component = std::variant<SourceQuote, Text>;

 public:
  template <typename... Ts>
  Diagnostic(Ts&&... ts) {
    components_.reserve(sizeof...(Ts));
    (components_.emplace_back(std::forward<Ts>(ts)), ...);
  }

  template <typename Fn>
  void for_each_component(Fn const& fn) const {
    for (auto const& component : components_) { std::visit(fn, component); }
  }

 private:
  std::vector<Component> components_;
};

}  // namespace diagnostic

#endif  // ICARUS_DIAGNOSTIC_DIAGNOSTIC_H
