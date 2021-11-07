#include "compiler/importer.h"


#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "base/debug.h"
#include "compiler/context.h"
#include "compiler/resources.h"
#include "compiler/work_graph.h"
#include "frontend/parse.h"
#include "frontend/source/file.h"
#include "frontend/source/file_name.h"
#include "frontend/source/shared.h"

namespace compiler {
namespace {

bool FileExists(std::string const& path) {
  if (std::FILE* f = std::fopen(path.c_str(), "r")) {
    std::fclose(f);
    return true;
  }
  return false;
}

// Looks up the given module path to retrieve an absolute path to the module.
frontend::CanonicalFileName ResolveModulePath(
    frontend::CanonicalFileName const& module_path,
    std::vector<std::string> const& lookup_paths) {
  // Respect absolute paths.
  if (absl::StartsWith(module_path.name(), "/")) { return module_path; }
  // Check for the module relative to the given lookup paths.
  for (std::string_view base_path : lookup_paths) {
    std::string path = absl::StrCat(base_path, "/", module_path.name());
    if (FileExists(path)) {
      return frontend::CanonicalFileName::Make(frontend::FileName(path));
    }
  }
  // Fall back to using the given path as-is, relative to $PWD.
  return module_path;
}

}  // namespace

ir::ModuleId FileImporter::Import(std::string_view module_locator) {
  auto file_name = frontend::CanonicalFileName::Make(
      frontend::FileName(std::string(module_locator)));
  auto [iter, inserted] = modules_.try_emplace(file_name);
  if (not inserted) { return iter->second->id; }

  auto maybe_file_src = frontend::FileSource::Make(
      ResolveModulePath(file_name, module_lookup_paths_));

  if (not maybe_file_src.ok()) {
    modules_.erase(iter);
    diagnostic_consumer_->Consume(frontend::MissingModule{
        .source    = file_name,
        .requestor = "",
        .reason    = std::string(maybe_file_src.status().message()),
    });
    return ir::ModuleId::Invalid();
  }

  iter->second                           = std::make_unique<ModuleData>();
  auto& [id, ir_module, context, module] = *iter->second;
  modules_by_id_.emplace(id, &module);

  for (ir::ModuleId embedded_id : implicitly_embedded_modules()) {
    module.embed(get(embedded_id));
  }

  auto nodes = module.InitializeNodes(
      frontend::Parse(maybe_file_src->buffer(), *diagnostic_consumer_));

  module.set_diagnostic_consumer<diagnostic::StreamingConsumer>(
      stderr, &*maybe_file_src);

  PersistentResources resources{
      .module              = &module,
      .diagnostic_consumer = &module.diagnostic_consumer(),
      .importer            = this,
  };
  CompileLibrary(context, resources, nodes);
  return id;
}

}  // namespace compiler