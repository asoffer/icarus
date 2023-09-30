#include <cstdio>
#include <fstream>
#include <optional>
#include <string>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "common/errno.h"
#include "common/string.h"
#include "diagnostics/consumer/streaming.h"
#include "diagnostics/message.h"
#include "ir/builtin_module.h"
#include "ir/emit.h"
#include "ir/ir.h"
#include "ir/module.pb.h"
#include "ir/serialize.h"
#include "lexer/lexer.h"
#include "nth/commandline/commandline.h"
#include "nth/debug/log/log.h"
#include "nth/io/file.h"
#include "nth/io/file_path.h"
#include "nth/process/exit_code.h"
#include "parser/parser.h"

namespace ic {
namespace {

std::optional<std::string> ReadFileToString(nth::file_path const& file_name) {
  errno_resetter e;
  std::optional<std::string> result = std::nullopt;
  std::optional<nth::file> file = nth::file::read_only(file_name);
  if (file) {
    std::fseek(file->get(), 0, SEEK_END);
    size_t file_size = file->tell();
    file->rewind();

    result.emplace();
    result->resize(file_size, '\0');
    (void)file->read_into(*result);
    NTH_REQUIRE(file->close());
  }
  return result;
}

nth::exit_code Compile(nth::FlagValueSet flags, nth::file_path const& source) {
  absl::InitializeSymbolizer("");
  absl::FailureSignalHandlerOptions opts;
  absl::InstallFailureSignalHandler(opts);

  auto const& output_path = flags.get<nth::file_path>("output");

  diag::StreamingConsumer consumer;
  std::optional content = ReadFileToString(source);
  if (not content) {
    consumer.Consume({
        diag::Header(diag::MessageKind::Error),
        diag::Text(
            InterpolateString<"Failed to load the content from {}.">(source)),
    });
    return nth::exit_code::generic_error;
  }

  consumer.set_source(*content);

  TokenBuffer token_buffer = Lex(*content, consumer);
  if (consumer.count() != 0) { return nth::exit_code::generic_error; }
  ParseTree parse_tree = Parse(token_buffer, consumer);
  if (consumer.count() != 0) { return nth::exit_code::generic_error; }
  consumer.set_parse_tree(parse_tree);

  Module module;
  IrContext ir_context = {
      .emit    = EmitContext(parse_tree, module),
  };
  ir_context.emit.modules = {
      BuiltinModule(token_buffer),
  };
  ir_context.ProcessIr(consumer);
  if (consumer.count() != 0) { return nth::exit_code::generic_error; }
  EmitIr(parse_tree.node_range(), ir_context.emit);
  ModuleProto module_proto = Serialize(module);

  std::ofstream out(output_path.path());
  return module_proto.SerializeToOstream(&out) ? nth::exit_code::success
                                               : nth::exit_code::generic_error;
}

}  // namespace
}  // namespace ic

nth::Usage const nth::program_usage = {
    .description = "Icarus Compiler",
    .flags =
        {
            {
                .name = {"output"},
                .type = nth::type<nth::file_path>,
                .description =
                    "The location at which to write the output .icm file.",

            },
        },
    .execute     = ic::Compile,
};
