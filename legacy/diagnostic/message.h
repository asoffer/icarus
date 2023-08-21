#ifndef ICARUS_DIAGNOSTIC_MESSAGE_H
#define ICARUS_DIAGNOSTIC_MESSAGE_H

#include <algorithm>
#include <span>
#include <string>
#include <variant>

#include "absl/strings/str_format.h"
#include "nth/meta/sequence.h"
#include "nth/meta/type.h"

namespace diagnostic {
// TODO Do you need `Fatal`?
enum class Category { Note, Warning, Error, Fatal };

struct Style {
  enum class Color : uint8_t {
    Black   = 0,
    Red     = 1,
    Green   = 2,
    Yellow  = 3,
    Blue    = 4,
    Magenta = 5,
    Cyan    = 6,
    White   = 7
  } color = Color::White;

  static constexpr Style ErrorText() { return Style{.color = Color::Red}; }
};

struct Highlight {
  Highlight(std::string_view range, Style style) : range(range), style(style) {}
  std::string_view range;
  Style style;
};

struct SourceQuote {
  SourceQuote& Highlighted(std::string_view range, Style style) {
    highlights.emplace_back(range, style);
    return *this;
  }

  std::vector<Highlight> highlights;
};

struct Text {
  explicit Text(std::string message) : message_(std::move(message)) {}
  template <typename... Args, std::enable_if_t<sizeof...(Args) != 0>* = nullptr>
  explicit Text(absl::FormatSpec<Args...> const& format, Args const&... args)
      : message_(absl::StrFormat(format, args...)) {}

  char const* c_str() const { return message_.c_str(); }
  operator std::string const&() const { return message_; }

 private:
  std::string message_;
};

struct List {
  explicit List(std::vector<std::string> items) : items_(std::move(items)) {}
  std::span<std::string const> items() const { return items_; }

 private:
  std::vector<std::string> items_;
};

struct DiagnosticMessage {
 private:
  using Component = std::variant<SourceQuote, Text, List>;

 public:
  template <typename... Ts>
  DiagnosticMessage(Ts&&... ts) {
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

#endif  // ICARUS_DIAGNOSTIC_MESSAGE_H