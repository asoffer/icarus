#ifndef ICARUS_DIAGNOSTIC_CONSOLE_RENDERER_H
#define ICARUS_DIAGNOSTIC_CONSOLE_RENDERER_H

#include <cstdio>
#include <type_traits>

#include "diagnostic/message.h"

namespace diagnostic {

struct ConsoleRenderer {
  // Assumes the file is already open.
  constexpr explicit ConsoleRenderer(std::FILE* out) : out_(out) {}

  void AddError(DiagnosticMessage const& diag) { Add(Category::Error, diag); }
  void AddError(frontend::Source const* source, DiagnosticMessage const& diag) {
    Add(source, Category::Error, diag);
  }

  void Add(Category cat, DiagnosticMessage const& diag);
  void Add(frontend::Source const* source, Category cat,
           DiagnosticMessage const& diag);
  void Flush();

 private:
  void WriteSourceQuote(frontend::Source const& source,
                        SourceQuote const& quote);

  bool has_data_ = false;
  std::FILE* out_;
};

}  // namespace diagnostic

#endif  // ICARUS_DIAGNOSTIC_CONSOLE_RENDERER_H
