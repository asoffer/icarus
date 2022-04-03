#ifndef ICARUS_AST_BUILD_PARAM_DEPENDENCY_GRAPH_H
#define ICARUS_AST_BUILD_PARAM_DEPENDENCY_GRAPH_H

#include <memory>

#include "ast/ast_fwd.h"
#include "base/graph.h"
#include "core/dependency_node.h"
#include "core/parameters.h"

namespace ast {

base::Graph<core::DependencyNode<Declaration>> BuildParamDependencyGraph(
    core::Parameters<Declaration> const &params);

}  // namespace ast

#endif  // ICARUS_AST_BUILD_PARAM_DEPENDENCY_GRAPH_H
